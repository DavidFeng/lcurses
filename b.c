#include <ncurses.h>      /* ncurses.h includes stdio.h */
#include <string.h>
#include <locale.h>

int main()
{
  const char * r2 = setlocale(LC_CTYPE, NULL);
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

                                    /* print the message at the center of the screen */
  mvprintw(row-2,0,"你好世界This screen has %d rows and %d columns\n",row,col);
  printw("Try resizing your window(if possible) and then run this program again");
  refresh();
  getch();
  endwin();
  printf("setlocale return: %s\n", r);
  printf("setlocale return 2: %s\n", r2);

  return 0;
}
