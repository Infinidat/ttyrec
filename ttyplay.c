/*
 * Copyright (c) 2000 Satoru Takabayashi <satoru@namazu.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by the University of
 *    California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define _GNU_SOURCE /* for memmem() */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <poll.h>
#include <math.h>

#include "ttyrec.h"
#include "io.h"
#include "ansi_codes.h"

typedef double (*WaitFunc)    (struct timeval prev,
                  struct timeval cur,
                  double speed);
typedef int     (*ReadFunc)    (FILE *fp, Header *h, char **buf);
typedef void    (*WriteFunc)    (char *buf, int len);
typedef void    (*ProcessFunc)    (FILE *fp, double speed,
                 ReadFunc read_func, WaitFunc wait_func);

static int paused;
static int quit;
static int new_line;

static int opt_times; //  = 1;
static int opt_dates;
static int opt_log_wait;
static int opt_utc;
static char* opt_time_format;

static int is_tty;
static int opt_colors = 1;

int set_progname(const char*);

static void usage(int do_exit);

struct timeval
timeval_diff (struct timeval tv1, struct timeval tv2)
{
    struct timeval diff;

    diff.tv_sec = tv2.tv_sec - tv1.tv_sec;
    diff.tv_usec = tv2.tv_usec - tv1.tv_usec;
    if (diff.tv_usec < 0) {
        diff.tv_sec--;
        diff.tv_usec += 1000000;
    }

    return diff;
}

struct timeval
timeval_div (struct timeval tv1, double n)
{
    double x = ((double)tv1.tv_sec  + (double)tv1.tv_usec / 1000000.0) / n;
    struct timeval div;

    div.tv_sec  = (int)x;
    div.tv_usec = (x - (int)x) * 1000000;

    return div;
}

double
ttywait (struct timeval prev, struct timeval cur, double speed)
{
    static struct timeval drift;
    struct timeval start;
    struct timeval diff;
#if 0
    fd_set readfs;
#else
    int diff_ms;
#endif
    // int paused = 0;

restart:
    drift.tv_sec = drift.tv_usec = 0;
    diff = timeval_diff(prev, cur);

    gettimeofday(&start, NULL);

    assert(speed != 0);
    diff = timeval_diff(drift, timeval_div(diff, speed));
    if (diff.tv_sec < 0) {
        diff.tv_sec = diff.tv_usec = 0;
    }

    diff_ms = diff.tv_sec * 1000 + diff.tv_usec / 1000;

    if (opt_log_wait) {
        if (diff_ms > 2000) {
            diff_ms = 1000 + (int)log2(diff_ms - 1000);
        }
    }

#if 0
    FD_SET(STDIN_FILENO, &readfs);

    /*
     * We use select() for sleeping with subsecond precision.
     * select() is also used to wait user's input from a keyboard.
     *
     * Save "diff" since select(2) may overwrite it to {0, 0}.
     */
    struct timeval orig_diff = diff;
    select(1, &readfs, NULL, NULL, &diff);
    diff = orig_diff;  /* Restore the original diff value. */
#else
    struct pollfd pollfd = { .fd = 0, .events = POLLIN };
    int n = poll(&pollfd, 1, diff_ms);
#endif

#if 0
    if (FD_ISSET(0, &readfs)) { /* a user hits a character? */
#else
    if (n == 1) {
#endif
        char c = 0;
        if (read(STDIN_FILENO, &c, 1) != 1)  /* drain the character */
            ;
        switch (c) {
        case '+':
        case 'f':
            speed *= 2;
            break;
        case 'n':
        case 'N':
            goto out;
            break;
        case 'q':
        case 'Q':
            quit = 1;
            break;
        case 't':
        case 'T':
            opt_times = !opt_times;
            break;
        case ' ':
            paused = !paused;
            break;
        case 'p':
            paused = 1;
            break;
        case 'r':
            paused = 0;
            break;
        case '-':
        case 's':
            speed /= 2;
            break;
        case '1':
            speed = 1.0;
            break;
        case '?':
        case 'h':
        case 'H':
            if (opt_colors) {
                fputs(term_save cursor_top, stdout);
                fputs("\n", stdout);
                usage(0);
                struct pollfd pollfd = { .fd = 0, .events = POLLIN };
                poll(&pollfd, 1, -1);
                fputs(term_restore, stdout);
            }
            break;
        }
        drift.tv_sec = drift.tv_usec = 0;
    } else {
        struct timeval stop;
        gettimeofday(&stop, NULL);
        /* Hack to accumulate the drift */
        if (diff.tv_sec == 0 && diff.tv_usec == 0) {
            diff = timeval_diff(drift, diff);  // diff = 0 - drift.
        }
        drift = timeval_diff(diff, timeval_diff(start, stop));
    }

    if (paused)
        goto restart;

out:
    return speed;
}

double
ttynowait (struct timeval prev, struct timeval cur, double speed)
{
    /* do nothing */
    return 0; /* Speed isn't important. */
}

int
ttyread (FILE *fp, Header *h, char **buf)
{
    if (read_header(fp, h) == 0) {
        return 0;
    }

    *buf = malloc(h->len);
    if (*buf == NULL) {
        perror("malloc");
    }

    if (fread(*buf, 1, h->len, fp) == 0) {
        perror("fread");
    }
    return 1;
}

int
ttypread (FILE *fp, Header *h, char **buf)
{
    /*
     * Read persistently just like tail -f.
     */
    while (ttyread(fp, h, buf) == 0) {
        poll(NULL, 0, 250);
        clearerr(fp);
    }
    return 1;
}

void
ttywrite (char *buf, int len)
{
    fwrite(buf, 1, len, stdout);
}

void
ttynowrite (char *buf, int len)
{
    /* do nothing */
}

static void print_time(Header* h)
{
    time_t sec = h->tv.tv_sec;
    struct tm tm;
    int n;

    static char tm_buff[256];
    static last_sec;

    if (opt_utc) {
        if (!gmtime_r(&sec, &tm)) {
            return;
        }
    } else {
        if (!localtime_r(&sec, &tm)) {
            return;
        }
    }

    n = strftime(tm_buff, sizeof(tm_buff), opt_time_format, &tm);
    if (n <= 0)
        return;

    fputs(tm_buff, stdout);
}

void
ttyplay (FILE *fp, double speed, ReadFunc read_func,
        WriteFunc write_func, WaitFunc wait_func)
{
    int first_time = 1;
    struct timeval prev;

    setbuf(stdout, NULL);
    setbuf(fp, NULL);

    while (!quit) {
        char *buf;
        Header h;

        if (read_func(fp, &h, &buf) == 0) {
            break;
        }

        if (!first_time) {
            speed = wait_func(prev, h.tv, speed);
        }
        first_time = 0;

        if (opt_times) {
            int n = h.len;
            char* p = buf;
            char* p_newline;

            while (n > 0) {
                if (new_line)
                    print_time(&h);
                p_newline = memmem(p, n, (void*)"\n", 1);
                if (p_newline) {
                    int delta = p_newline - p  + 1;
                    write_func(p, delta);
                    p += delta;
                    n -= delta;
                    new_line = 1;
                } else {
                    write_func(p, n);
                    new_line = 0;
                    break;
                }
            }
        } else {
            write_func(buf, h.len);
        }
        prev = h.tv;
        free(buf);
    }
}

void
ttyskipall (FILE *fp)
{
    /*
     * Skip all records.
     */
    ttyplay(fp, 0, ttyread, ttynowrite, ttynowait);
}

void ttyplayback (FILE *fp, double speed,
        ReadFunc read_func, WaitFunc wait_func)
{
    ttyplay(fp, speed, ttyread, ttywrite, wait_func);
}

void ttypeek (FILE *fp, double speed,
        ReadFunc read_func, WaitFunc wait_func)
{
    ttyskipall(fp);
    ttyplay(fp, speed, ttypread, ttywrite, ttynowait);
}

static void
show_version (void)
{
    printf("\n");
    printf("git@git.infinidat.com:sagig/ttyrec.git\n");
    printf("\n");
    printf("Compiled on: %s %s\n", __DATE__, __TIME__);
    printf("\n");

    exit(0);
}

static void
usage (int do_exit)
{
    printf("Usage: ttyplay [OPTION] [FILE]\n");
    printf("  -s SPEED Set speed to SPEED [1.0]\n");
    printf("  -n       No wait mode\n");
    printf("  -l       Logarithmic wait mode\n");
    printf("  -p       Peek another person's ttyrecord\n");
    printf("  -t       Print time (default)\n");
    printf("  -T       Do not print time\n");
    printf("  -u       Print time in UTC\n");
    printf("  -d       Print date and time\n");
    printf("  -f FMT   Time format (strftime); default: '[%%T] '\n");
    printf("  -C       No colors\n");
    printf("  -V       Version\n");
    printf("  -h       Usage\n");
    printf("\n");
    printf("Keys during playback:\n");
    printf("  '+/f'    double speed\n");
    printf("  '-/s'    half speed\n");
    printf("  '1'      original speed\n");
    printf("  ' '      toggle pause/resume\n");
    printf("  't'      toggle times\n");
    printf("  'p'      pause\n");
    printf("  'r'      resume\n");
    printf("  'n'      next frame\n");
    printf("  'q'      quit\n");
    printf("  'h/?'    help\n");
    printf("\n");
    if (do_exit)
        exit(EXIT_FAILURE);
}

/*
 * We do some tricks so that select(2) properly works on
 * STDIN_FILENO in ttywait().
 */
FILE *
input_from_stdin (void)
{
    FILE *fp;
    int fd = edup(STDIN_FILENO);
    edup2(STDOUT_FILENO, STDIN_FILENO);
    return efdopen(fd, "r");
}

static char*
colored_format(const char* format)
{
    char* result = malloc(strlen(format) + 64);

    strcat(result, cl_normal);
    strcat(result, fg_green_bold);
    strcat(result, format);
    strcat(result, cl_normal);

    return result;
}

int
main (int argc, char **argv)
{
    double speed = 1.0;
    ReadFunc read_func  = ttyread;
    WaitFunc wait_func  = ttywait;
    ProcessFunc process = ttyplayback;
    FILE *input = NULL;
    struct termios old, new;

    set_progname(argv[0]);
    while (1) {
        int ch = getopt(argc, argv, "s:nptTCuf:dlVh");
        if (ch == EOF) {
            break;
        }
        switch (ch) {
            case 's':
                if (optarg == NULL) {
                    perror("-s option requires an argument");
                    exit(EXIT_FAILURE);
                }
                sscanf(optarg, "%lf", &speed);
                break;
            case 'n':
                wait_func = ttynowait;
                break;
            case 'p':
                opt_times = 0;
                process = ttypeek;
                break;
            case 'T':
                opt_times = 0;
                break;
            case 't':
                opt_times = 1;
                break;
            case 'd':
                opt_times = 1;
                opt_dates = 1;
                break;
            case 'u':
                opt_times = 1;
                opt_utc = 1;
                break;
            case 'l':
                opt_log_wait = 1;
                break;
            case 'f':
                opt_time_format = strdup(optarg);
                break;
            case 'C':
                opt_colors = 0;
                break;
            case 'V':
                show_version();
                break;
            case 'h':
            default:
                usage(1);
        }
    }

    if (optind < argc) {
        input = efopen(argv[optind], "r");
    } else {
        input = input_from_stdin();
    }
    assert(input != NULL);

    is_tty = isatty(1);
    if (!is_tty)
        opt_colors = 0;

    if (!opt_time_format)
        opt_time_format = opt_dates ? "[%F %T] " : "[%T] ";

    if (opt_colors)
        opt_time_format = colored_format(opt_time_format);

    if (opt_times)
        new_line = 1;

    tcgetattr(0, &old); /* Get current terminal state */
    new = old;          /* Make a copy */
    new.c_lflag &= ~(ICANON | ECHO | ECHONL); /* unbuffered, no echo */
    tcsetattr(0, TCSANOW, &new); /* Make it current */

    process(input, speed, read_func, wait_func);
    tcsetattr(0, TCSANOW, &old);  /* Return terminal state */

    return 0;
}
