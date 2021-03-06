# makefile for lcurses library for Lua

# dist location
DISTDIR=$(HOME)/dist
TMP=/tmp

# change these to reflect your Lua installation
LUAINC= /home/david/david/skynet/3rd/lua

CC = gcc

# no need to change anything below here
SHFLAGS= -shared
CFLAGS= -std=gnu99 -g $(INCS) $(DEFS) $(WARN) $(SHFLAGS) -O2 -fPIC
DEFS=
WARN= -Wall
INCS= -I$(LUAINC)
LIBS= -lpanel -lncursesw

MYNAME= curses
MYLIB= l$(MYNAME)

OBJS= $(MYLIB).o

T= $(MYLIB).so

VER=0.2.1
TARFILE = $(DISTDIR)/$(MYLIB)-$(VER).tar.gz
TARFILES = \
	README Makefile \
	lcurses.c lpanel.c \
	lcurses.html \
	requireso.lua curses.lua curses.panel.lua \
	test.lua \
	cui.lua cui.ctrls.lua testcui.lua \
	firework.lua interp.lua

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
SHFLAGS= -shared
endif
ifeq ($(UNAME), Darwin)
SHFLAGS= -bundle -undefined dynamic_lookup
endif

all: $T

lua: lcurses.c lua.c
	gcc -I. -DDEBUG -g -o lua lua.c lcurses.c -L. -llualib -llua -lcursesw -lpanel -lm -ldl

cui: $T
	$(LUABIN)/lua -l$(MYNAME) -l$(MYNAME).panel testcui.lua

test:	$T
	$(LUABIN)/lua -l$(MYNAME) -l$(MYNAME).panel test.lua

$T:	$(OBJS)
	$(CC) $(SHFLAGS) -o $@  $(OBJS) $(LIBS)

lcurses.o: lcurses.c lpanel.c

c :
	gcc -std=c99 -I/home/david/david/skynet/3rd/lua  c.c -L/home/david/david/skynet/3rd/lua -llua -ldl -lm

clean:
	rm -f $(OBJS) $T core core.* a.out

dist:
	@echo 'Exporting...'
	@cvs export -r HEAD -d $(TMP)/$(MYLIB)-$(VER) $(MYLIB)
	@echo 'Compressing...'
	@tar -zcf $(TARFILE) -C $(TMP) $(MYLIB)-$(VER)
	@rm -fr $(TMP)/$(MYLIB)-$(VER)
	@lsum $(TARFILE) $(DISTDIR)/md5sums.txt
	@echo 'Done.'
