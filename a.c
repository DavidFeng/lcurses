//#include <curses.h>
#include <ncursesw/ncurses.h>
#include <locale.h>

int main()
{
    setlocale(LC_ALL, "");
    initscr();
    mvaddstr(5,10,"你好世界 Hello 中文支持");
    getch();
    endwin();
    return 0;
}
