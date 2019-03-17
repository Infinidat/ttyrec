VERSION = 1.0.8-v2

CC = gcc
CFLAGS = -O2 -g

TARGET = ttyrec ttyplay ttytime

DIST =	ttyrec.c ttyplay.c ttyrec.h io.c io.h ttytime.c \
	README Makefile ttyrec.1 ttyplay.1 ttytime.1

all: $(TARGET)

io.o: build.h

ttyrec: ttyrec.o io.o
	$(CC) $(CFLAGS) -o ttyrec ttyrec.o io.o -lutil

ttyplay: ttyplay.o io.o
	$(CC) $(CFLAGS) -o ttyplay ttyplay.o io.o -lm -g

ttytime: ttytime.o io.o
	$(CC) $(CFLAGS) -o ttytime ttytime.o io.o

clean:
	rm -f *.o $(TARGET) ttyrecord *~

dist:
	rm -rf ttyrec-$(VERSION)
	rm -f ttyrec-$(VERSION).tar.gz

	mkdir ttyrec-$(VERSION)
	cp $(DIST) ttyrec-$(VERSION)
	tar zcf ttyrec-$(VERSION).tar.gz  ttyrec-$(VERSION)
	rm -rf ttyrec-$(VERSION)

build.h: Makefile .git/
	@echo "/* Auto-generated file */" > $@
	@echo '#define HOSTNAME "$(shell hostname)"' >> $@
	@echo '#define USERNAME "$(shell id -u -n)"' >> $@
	@echo '#define TTYREC_VERSION "$(VERSION)"'  >> $@
	@echo '#define GIT_REVISION "$(shell git rev-parse HEAD)"' >> $@
	@echo "======= $@ ========"
	@cat $@
	@echo "------- $@ --------"
