#ifndef __COLORS__H
#define __COLORS__H

#define fg_black "\033[30m"
#define fg_red "\033[31m"
#define fg_green "\033[32m"
#define fg_yellow "\033[33m"
#define fg_blue "\033[34m"
#define fg_magenta "\033[35m"
#define fg_cyan "\033[36m"
#define fg_white "\033[37m"

#define fg_black_bold "\033[30;1m"
#define fg_red_bold "\033[31;1m"
#define fg_green_bold "\033[32;1m"
#define fg_yellow_bold "\033[33;1m"
#define fg_blue_bold "\033[34;1m"
#define fg_magenta_bold "\033[35;1m"
#define fg_cyan_bold "\033[36;1m"
#define fg_white_bold "\033[37;1m"

#define bg_black "\e[40m"
#define bg_red "\e[41m"
#define bg_green "\e[42m"
#define bg_yellow "\e[43m"
#define bg_blue "\e[44m"
#define bg_magenta "\e[45m"
#define bg_cyan "\e[46m"
#define bg_white "\e[47m"

#define cl_normal "\033[0m"

#define term_save "\033[?1049h"
#define term_restore "\033[?1049h"

#define cursor_top "\033[0;0H"

#endif /* __COLORS__H */
