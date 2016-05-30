#define _XOPEN_SOURCE_EXTENDED

#include <ncursesw/ncurses.h>      /* ncurses.h includes stdio.h */
#include <string.h>
#include <locale.h>

int main()
{
  const char * r = setlocale(LC_CTYPE, "");

  char mesg[]="Just a string";   /* message to be appeared on the screen */
  int row,col;       /* to store the number of rows and *
            * the number of colums of the screen */
  initscr();       /* start the curses mode */
  getmaxyx(stdscr,row,col);    /* get the number of rows and columns */
  mvprintw(row/2,(col-strlen(mesg))/2,"%s",mesg);

  mvprintw(14, 1, "%s","1234567890");

  mvprintw(15, 0, "%s","1234567890");
  mvprintw(16, 0, "%s","一二三四五六七八九十1234567890");

  mvprintw(15, 30, "%s","hello abc");
  mvprintw(16, 30, "%s","给你一张过去的CD");

  cchar_t c[] = {
    {0, L"你"},
    {A_BLINK, L"好"},
    {A_UNDERLINE, L"世"},
    {A_COLOR, L"界"},
    {0, L"你"},
    {A_REVERSE, L"好"},
    {A_LEFT, L"世"},
    {A_STANDOUT, L"界"},
    {0, 0}
  };
  mvadd_wchstr(18, 30, c);

                                    /* print the message at the center of the screen */
  mvprintw(row-2,0,"你好世界This screen has %d rows and %d columns\n",row,col);
  printw("Try resizing your window(if possible) and then run this program again");
  refresh();
  getch();
  endwin();
  printf("setlocale return: %s\n", r);

  wchar_t c1 = L'中';
  wchar_t cs[] = L"中文世界";
  printf("%d %d: %lc %ls\n", sizeof(c1), c1, c1, cs);


  return 0;
}
