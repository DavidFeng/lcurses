/************************************************************************
* Author    : Tiago Dionizio (tngd@mega.ist.utl.pt)                     *
* Library   : lcurses - Lua 5 interface to the curses library           *
*                                                                       *
************************************************************************/

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"

#include <wchar.h>

#define _XOPEN_SOURCE_EXTENDED
#include <ncursesw/ncurses.h>
#include <signal.h>

/*
** =======================================================
** addon libraries
** =======================================================
*/
#define INCLUDEPANEL 1

/*
** =======================================================
** defines
** =======================================================
*/
static const char *STDSCR_REGISTRY     = "curses:stdscr";
static const char *WINDOWMETA          = "curses:window";
static const char *CHSTRMETA           = "curses:chstr";
static const char *RIPOFF_TABLE        = "curses:ripoffline";

#define B(v) ((v == ERR) ? 0 : 1)

#define CCHTYPE_CAST

/* ======================================================= */

#define LC_NUMBER(v)                        \
    static int lc_ ## v(lua_State *L)       \
    {                                       \
        lua_pushnumber(L, v());             \
        return 1;                           \
    }

#define LC_NUMBER2(n,v)                     \
    static int lc_ ## n(lua_State *L)       \
    {                                       \
        lua_pushnumber(L, v);               \
        return 1;                           \
    }

/* ======================================================= */

#define LC_STRING(v)                        \
    static int lc_ ## v(lua_State *L)       \
    {                                       \
        lua_pushstring(L, v());             \
        return 1;                           \
    }

#define LC_STRING2(n,v)                     \
    static int lc_ ## n(lua_State *L)       \
    {                                       \
        lua_pushstring(L, v);               \
        return 1;                           \
    }

/* ======================================================= */

#define LC_BOOL(v)                          \
    static int lc_ ## v(lua_State *L)       \
    {                                       \
        lua_pushboolean(L, v());            \
        return 1;                           \
    }

#define LC_BOOL2(n,v)                       \
    static int lc_ ## n(lua_State *L)       \
    {                                       \
        lua_pushboolean(L, v);              \
        return 1;                           \
    }

/* ======================================================= */

#define LC_BOOLOK(v)                        \
    static int lc_ ## v(lua_State *L)       \
    {                                       \
        lua_pushboolean(L, B(v()));         \
        return 1;                           \
    }

#define LC_BOOLOK2(n,v)                     \
    static int lc_ ## n(lua_State *L)       \
    {                                       \
        lua_pushboolean(L, B(v));           \
        return 1;                           \
    }

/* ======================================================= */

#define LCW_BOOLOK(n)                       \
    static int lcw_ ## n(lua_State *L)      \
    {                                       \
        WINDOW *w = lcw_check(L, 1);        \
        lua_pushboolean(L, B(n(w)));        \
        return 1;                           \
    }


static int lc_map_keyboard(lua_State *L)
{
    lua_pushboolean(L, 1);
    return 1;
}

/*
** =======================================================
** privates
** =======================================================
*/
static void lcw_new(lua_State *L, WINDOW *nw)
{
    if (nw)
    {
        WINDOW **w = lua_newuserdata(L, sizeof(WINDOW*));
        luaL_getmetatable(L, WINDOWMETA);
        lua_setmetatable(L, -2);
        *w = nw;
    }
    else
    {
        lua_pushliteral(L, "failed to create window");
        lua_error(L);
    }
}

static WINDOW **lcw_get(lua_State *L, int index)
{
    WINDOW **w = (WINDOW**)luaL_checkudata(L, index, WINDOWMETA);
    if (w == NULL) luaL_argerror(L, index, "bad curses window");
    return w;
}

static WINDOW *lcw_check(lua_State *L, int index)
{
    WINDOW **w = lcw_get(L, index);
    if (*w == NULL) luaL_argerror(L, index, "attempt to use closed curses window");
    return *w;
}

static int lcw_tostring(lua_State *L)
{
    WINDOW **w = lcw_get(L, 1);
    char buff[34];
    if (*w == NULL)
        strcpy(buff, "closed");
    else
        sprintf(buff, "%p", lua_touserdata(L, 1));
    lua_pushfstring(L, "curses window (%s)", buff);
    return 1;
}

/*
** =======================================================
** cchar_t handling
** =======================================================
*/
static cchar_t lc_checkch(lua_State *L, int index)
{
  cchar_t r = {.attr = 0 };
  if (lua_type(L, index) == LUA_TNUMBER)
    r.chars[0] = luaL_checknumber(L, index);
    return r;
  if (lua_type(L, index) == LUA_TSTRING)
    r.chars[0] = *lua_tostring(L, index);
    return r;

  luaL_error(L, "type error");
  /* never executes */
  return r;
}

static chtype lc_checkchtype(lua_State *L, int index)
{
  if (lua_type(L, index) == LUA_TNUMBER)
    return luaL_checknumber(L, index);
  if (lua_type(L, index) == LUA_TSTRING)
    return *lua_tostring(L, index);;

  luaL_error(L, "type error");
  /* never executes */
  return 0;
}

static chtype lc_opt_chtype(lua_State *L, int index, chtype def) {
  return lua_isnoneornil(L, index) ? def : lc_checkchtype(L, index);
}

/*
** =======================================================
** chstr handling
** =======================================================
*/
typedef struct
{
    unsigned int len;
    cchar_t str[1];
} chstr;
#define CHSTR_SIZE(len) (sizeof(chstr) + len * sizeof(cchar_t))


/* create new chstr object and leave it in the lua stack */
static chstr* chstr_new(lua_State *L, int len)
{
  if (len < 1) {
    lua_pushliteral(L, "invalid chstr length");
    lua_error(L);
  }
  chstr *cs = lua_newuserdata(L, CHSTR_SIZE(len));
  luaL_getmetatable(L, CHSTRMETA);
  lua_setmetatable(L, -2);
  cs->len = len;
  return cs;
}

/* get chstr from lua (convert if needed) */
static chstr* lc_checkchstr(lua_State *L, int index)
{
    chstr *cs = (chstr*)luaL_checkudata(L, index, CHSTRMETA);
    if (cs) return cs;

    luaL_argerror(L, index, "bad curses chstr");
    return NULL;
}

/* create a new curses str */
static int lc_new_chstr(lua_State *L)
{
  int len = luaL_checkinteger(L, 1);
  chstr* ncs = chstr_new(L, len);
  memset(ncs->str, 0, len * sizeof(cchar_t));
  return 1;
}

/* change the contents of the chstr */
static int chstr_set_str(lua_State *L) {
  chstr *cs = lc_checkchstr(L, 1);
  int index = luaL_checkinteger(L, 2);

  attr_t attr = (attr_t)luaL_optnumber(L, 4, A_NORMAL);

  int i = 0;

  if (index < 0) return 0;

  int n = lua_gettop(L);
  lua_getglobal(L, "utf8");
  lua_getfield(L, -1, "codes");
  lua_remove(L, -2); // remove global utf8
  lua_pushvalue(L, 3);

  lua_call(L, 1, LUA_MULTRET);
  while (1) {
    lua_pushvalue(L, -3);
    lua_insert(L, -3);
    lua_pushvalue(L, -2);
    lua_insert(L, -4);

    lua_call(L, 2, LUA_MULTRET);
    if (lua_gettop(L) == n + 2) {
      lua_pop(L, 2);
      break;
    } else {
      int c = lua_tointeger(L, -1);
      cs->str[i].chars[0] = c;
      //cs->str[i].chars[0] = 'A';
      cs->str[i++].attr = attr;
      lua_pop(L, 1);
    }
  }

  return 0;
}

/* change the contents of the chstr */
static int chstr_set_ch(lua_State *L)
{
  chstr* cs = lc_checkchstr(L, 1);
  int index = luaL_checkinteger(L, 2);
  cchar_t ch = lc_checkch(L, 3);
  int attr = (chtype)luaL_optnumber(L, 4, A_NORMAL);

  cs->str[index] = ch;
  cs->str[index].attr = attr;
  return 0;
}

/* get information from the chstr */
static int chstr_get(lua_State *L)
{
    chstr* cs = lc_checkchstr(L, 1);
    int index = luaL_checkinteger(L, 2);
    cchar_t ch;

    if (index < 0 || index >= cs->len)
        return 0;

    ch = cs->str[index];

    lua_pushnumber(L, ch.chars[0]);
    attr_t a = ch.attr;
    lua_pushnumber(L, a & A_ATTRIBUTES);
    lua_pushnumber(L, a & A_COLOR);
    return 3;
}

/* retrieve chstr length */
static int chstr_len(lua_State *L)
{
    chstr *cs = lc_checkchstr(L, 1);
    lua_pushnumber(L, cs->len);
    return 1;
}

/* duplicate chstr */
static int chstr_dup(lua_State *L)
{
    chstr *cs = lc_checkchstr(L, 1);
    chstr *ncs = chstr_new(L, cs->len);

    memcpy(ncs->str, cs->str, sizeof(cchar_t) * (cs->len));
    return 1;
}

/*
** =======================================================
** initscr
** =======================================================
*/

#define CCR(n, v)                       \
    lua_pushstring(L, n);               \
    lua_pushnumber(L, v);               \
    lua_rawset(L, lua_upvalueindex(1));

#define CC(s)       CCR(#s, s)
#define CC2(s, v)   CCR(#s, v)

/*
** these values may be fixed only after initialization, so this is
** called from lc_initscr, after the curses driver is initialized
**
** curses table is kept at upvalue position 1, in case the global
** name is changed by the user or even in the registration phase by
** the developer
**
** some of these values are not constant so need to register
** them directly instead of using a table
*/
static void register_curses_constants(lua_State *L)
{
    /* colors */
    CC(COLOR_BLACK)     CC(COLOR_RED)       CC(COLOR_GREEN)
    CC(COLOR_YELLOW)    CC(COLOR_BLUE)      CC(COLOR_MAGENTA)
    CC(COLOR_CYAN)      CC(COLOR_WHITE)

    /* alternate character set */
    CC(ACS_BLOCK)       CC(ACS_BOARD)

    CC(ACS_BTEE)        CC(ACS_TTEE)
    CC(ACS_LTEE)        CC(ACS_RTEE)
    CC(ACS_LLCORNER)    CC(ACS_LRCORNER)
    CC(ACS_URCORNER)    CC(ACS_ULCORNER)

    CC(ACS_LARROW)      CC(ACS_RARROW)
    CC(ACS_UARROW)      CC(ACS_DARROW)

    CC(ACS_HLINE)       CC(ACS_VLINE)

    CC(ACS_BULLET)      CC(ACS_CKBOARD)     CC(ACS_LANTERN)
    CC(ACS_DEGREE)      CC(ACS_DIAMOND)

    CC(ACS_PLMINUS)     CC(ACS_PLUS)
    CC(ACS_S1)          CC(ACS_S9)

    /* attributes */
    CC(A_NORMAL)        CC(A_STANDOUT)      CC(A_UNDERLINE)
    CC(A_REVERSE)       CC(A_BLINK)         CC(A_DIM)
    CC(A_BOLD)          CC(A_PROTECT)       CC(A_INVIS)
    CC(A_ALTCHARSET)    CC(A_CHARTEXT)

    /* key functions */
    CC(KEY_BREAK)       CC(KEY_DOWN)        CC(KEY_UP)
    CC(KEY_LEFT)        CC(KEY_RIGHT)       CC(KEY_HOME)
    CC(KEY_BACKSPACE)

    CC(KEY_DL)          CC(KEY_IL)          CC(KEY_DC)
    CC(KEY_IC)          CC(KEY_EIC)         CC(KEY_CLEAR)
    CC(KEY_EOS)         CC(KEY_EOL)         CC(KEY_SF)
    CC(KEY_SR)          CC(KEY_NPAGE)       CC(KEY_PPAGE)
    CC(KEY_STAB)        CC(KEY_CTAB)        CC(KEY_CATAB)
    CC(KEY_ENTER)       CC(KEY_SRESET)      CC(KEY_RESET)
    CC(KEY_PRINT)       CC(KEY_LL)          CC(KEY_A1)
    CC(KEY_A3)          CC(KEY_B2)          CC(KEY_C1)
    CC(KEY_C3)          CC(KEY_BTAB)        CC(KEY_BEG)
    CC(KEY_CANCEL)      CC(KEY_CLOSE)       CC(KEY_COMMAND)
    CC(KEY_COPY)        CC(KEY_CREATE)      CC(KEY_END)
    CC(KEY_EXIT)        CC(KEY_FIND)        CC(KEY_HELP)
    CC(KEY_MARK)        CC(KEY_MESSAGE)     CC(KEY_MOUSE)
    CC(KEY_MOVE)        CC(KEY_NEXT)        CC(KEY_OPEN)
    CC(KEY_OPTIONS)     CC(KEY_PREVIOUS)    CC(KEY_REDO)
    CC(KEY_REFERENCE)   CC(KEY_REFRESH)     CC(KEY_REPLACE)
    CC(KEY_RESIZE)      CC(KEY_RESTART)     CC(KEY_RESUME)
    CC(KEY_SAVE)        CC(KEY_SBEG)        CC(KEY_SCANCEL)
    CC(KEY_SCOMMAND)    CC(KEY_SCOPY)       CC(KEY_SCREATE)
    CC(KEY_SDC)         CC(KEY_SDL)         CC(KEY_SELECT)
    CC(KEY_SEND)        CC(KEY_SEOL)        CC(KEY_SEXIT)
    CC(KEY_SFIND)       CC(KEY_SHELP)       CC(KEY_SHOME)
    CC(KEY_SIC)         CC(KEY_SLEFT)       CC(KEY_SMESSAGE)
    CC(KEY_SMOVE)       CC(KEY_SNEXT)       CC(KEY_SOPTIONS)
    CC(KEY_SPREVIOUS)   CC(KEY_SPRINT)      CC(KEY_SREDO)
    CC(KEY_SREPLACE)    CC(KEY_SRIGHT)      CC(KEY_SRSUME)
    CC(KEY_SSAVE)       CC(KEY_SSUSPEND)    CC(KEY_SUNDO)
    CC(KEY_SUSPEND)     CC(KEY_UNDO)

    /* KEY_Fx  0 <= x <= 63 */
    CC(KEY_F0)              CC2(KEY_F1, KEY_F(1))   CC2(KEY_F2, KEY_F(2))
    CC2(KEY_F3, KEY_F(3))   CC2(KEY_F4, KEY_F(4))   CC2(KEY_F5, KEY_F(5))
    CC2(KEY_F6, KEY_F(6))   CC2(KEY_F7, KEY_F(7))   CC2(KEY_F8, KEY_F(8))
    CC2(KEY_F9, KEY_F(9))   CC2(KEY_F10, KEY_F(10)) CC2(KEY_F11, KEY_F(11))
    CC2(KEY_F12, KEY_F(12))
}

/*
** make sure screen is restored (and cleared) at exit
** (for the situations where program is aborted without a
** proper cleanup)
*/
static void cleanup()
{
    if (!isendwin())
    {
        wclear(stdscr);
        wrefresh(stdscr);
        endwin();
    }
}

static int lc_initscr(lua_State *L)
{
    WINDOW *w;

    /* initialize curses */
    w = initscr();

    /* no longer used, so clean it up */
    lua_pushstring(L, RIPOFF_TABLE);
    lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);

    /* failed to initialize */
    if (w == NULL)
        return 0;

    #if defined(NCURSES_VERSION)
    /* acomodate this value for cui keyboard handling */
    ESCDELAY = 0;
    #endif

    /* return stdscr - main window */
    lcw_new(L, w);

    /* save main window on registry */
    lua_pushstring(L, STDSCR_REGISTRY);
    lua_pushvalue(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);

    /* setup curses constants - curses.xxx numbers */
    register_curses_constants(L);

    /* install cleanup handler to help in debugging and screen trashing */
    atexit(cleanup);
    /* disable interrupt signal
    signal(SIGINT, SIG_IGN);
    signal(SIGBREAK, SIG_IGN);
    signal(SIGTERM, SIG_IGN);*/
    return 1;
}

static int lc_endwin(lua_State *L)
{
    endwin();
#ifdef XCURSES
    XCursesExit();
    exit(0);
#endif
    return 0;
}

LC_BOOL(isendwin)

static int lc_stdscr(lua_State *L)
{
    lua_pushstring(L, STDSCR_REGISTRY);
    lua_rawget(L, LUA_REGISTRYINDEX);
    return 1;
}

LC_NUMBER2(COLS, COLS)
LC_NUMBER2(LINES, LINES)

/*
** =======================================================
** color
** =======================================================
*/

LC_BOOLOK(start_color)
LC_BOOL(has_colors)

static int lc_init_pair(lua_State *L)
{
    short pair = luaL_checkinteger(L, 1);
    short f = luaL_checkinteger(L, 2);
    short b = luaL_checkinteger(L, 3);

    lua_pushboolean(L, B(init_pair(pair, f, b)));
    return 1;
}

static int lc_pair_content(lua_State *L)
{
    short pair = luaL_checkinteger(L, 1);
    short f;
    short b;
    int ret = pair_content(pair, &f, &b);

    if (ret == ERR)
        return 0;

    lua_pushnumber(L, f);
    lua_pushnumber(L, b);
    return 2;
}

LC_NUMBER2(COLORS, COLORS)
LC_NUMBER2(COLOR_PAIRS, COLOR_PAIRS)

static int lc_COLOR_PAIR(lua_State *L)
{
    int n = luaL_checkinteger(L, 1);
    lua_pushnumber(L, COLOR_PAIR(n));
    return 1;
}

/*
** =======================================================
** termattrs
** =======================================================
*/

LC_NUMBER(baudrate)
LC_NUMBER(erasechar)
LC_BOOL(has_ic)
LC_BOOL(has_il)
LC_NUMBER(killchar)

static int lc_termattrs(lua_State *L)
{
    if (lua_gettop(L) < 1)
    {
        lua_pushnumber(L, termattrs());
    }
    else
    {
        int a = luaL_checkinteger(L, 1);
        lua_pushboolean(L, termattrs() & a);
    }
    return 1;
}

LC_STRING(termname)
LC_STRING(longname)

/*
** =======================================================
** kernel
** =======================================================
*/

/* there is no easy way to implement this... */
static lua_State *rip_L = NULL;
static int ripoffline_cb(WINDOW* w, int cols)
{
    static int line = 0;
    int top = lua_gettop(rip_L);

    /* better be safe */
    if (!lua_checkstack(rip_L, 5))
        return 0;

    /* get the table from the registry */
    lua_pushstring(rip_L, RIPOFF_TABLE);
    lua_gettable(rip_L, LUA_REGISTRYINDEX);

    /* get user callback function */
    if (lua_isnil(rip_L, -1)) {
        lua_pop(rip_L, 1);
        return 0;
    }

    lua_rawgeti(rip_L, -1, ++line); /* function to be called */
    lcw_new(rip_L, w);              /* create window object */
    lua_pushnumber(rip_L, cols);    /* push number of columns */

    lua_pcall(rip_L, 2,  0, 0);     /* call the lua function */

    lua_settop(rip_L, top);
    return 1;
}

static int lc_ripoffline(lua_State *L)
{
    static int rip = 0;
    int top_line = lua_toboolean(L, 1);

    if (!lua_isfunction(L, 2))
    {
        lua_pushliteral(L, "invalid callback passed as second parameter");
        lua_error(L);
    }

    /* need to save the lua state somewhere... */
    rip_L = L;

    /* get the table where we are going to save the callbacks */
    lua_pushstring(L, RIPOFF_TABLE);
    lua_gettable(L, LUA_REGISTRYINDEX);

    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        lua_newtable(L);

        lua_pushstring(L, RIPOFF_TABLE);
        lua_pushvalue(L, -2);
        lua_settable(L, LUA_REGISTRYINDEX);
    }

    /* save function callback in registry table */
    lua_pushvalue(L, 2);
    lua_rawseti(L, -2, ++rip);

    /* and tell curses we are going to take the line */
    lua_pushboolean(L, B(ripoffline(top_line ? 1 : -1, ripoffline_cb)));
    return 1;
}

static int lc_curs_set(lua_State *L)
{
    int vis = luaL_checkinteger(L, 1);
    int state = curs_set(vis);
    if (state == ERR)
        return 0;

    lua_pushnumber(L, state);
    return 1;
}

static int lc_napms(lua_State *L)
{
    int ms = luaL_checkinteger(L, 1);
    lua_pushboolean(L, B(napms(ms)));
    return 1;
}

/*
** =======================================================
** beep
** =======================================================
*/
LC_BOOLOK(beep)
LC_BOOLOK(flash)


/*
** =======================================================
** window
** =======================================================
*/

static int lc_newwin(lua_State *L)
{
    int nlines  = luaL_checkinteger(L, 1);
    int ncols   = luaL_checkinteger(L, 2);
    int begin_y = luaL_checkinteger(L, 3);
    int begin_x = luaL_checkinteger(L, 4);

    lcw_new(L, newwin(nlines, ncols, begin_y, begin_x));
    return 1;
}

static int lcw_delwin(lua_State *L)
{
    WINDOW **w = lcw_get(L, 1);
    if (*w != NULL && *w != stdscr)
    {
        delwin(*w);
        *w = NULL;
    }
    return 0;
}

static int lcw_mvwin(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkinteger(L, 2);
    int x = luaL_checkinteger(L, 3);
    lua_pushboolean(L, B(mvwin(w, y, x)));
    return 1;
}

static int lcw_subwin(lua_State *L)
{
    WINDOW *orig = lcw_check(L, 1);
    int nlines  = luaL_checkinteger(L, 2);
    int ncols   = luaL_checkinteger(L, 3);
    int begin_y = luaL_checkinteger(L, 4);
    int begin_x = luaL_checkinteger(L, 5);

    lcw_new(L, subwin(orig, nlines, ncols, begin_y, begin_x));
    return 1;
}

static int lcw_derwin(lua_State *L)
{
    WINDOW *orig = lcw_check(L, 1);
    int nlines  = luaL_checkinteger(L, 2);
    int ncols   = luaL_checkinteger(L, 3);
    int begin_y = luaL_checkinteger(L, 4);
    int begin_x = luaL_checkinteger(L, 5);

    lcw_new(L, derwin(orig, nlines, ncols, begin_y, begin_x));
    return 1;
}

static int lcw_mvderwin(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int par_y = luaL_checkinteger(L, 2);
    int par_x = luaL_checkinteger(L, 3);
    lua_pushboolean(L, B(mvderwin(w, par_y, par_x)));
    return 1;
}

static int lcw_dupwin(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    lcw_new(L, dupwin(w));
    return 1;
}

static int lcw_wsyncup(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    wsyncup(w);
    return 0;
}

static int lcw_syncok(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int bf = lua_toboolean(L, 2);
    lua_pushboolean(L, B(syncok(w, bf)));
    return 1;
}

static int lcw_wcursyncup(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    wcursyncup(w);
    return 0;
}

static int lcw_wsyncdown(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    wsyncdown(w);
    return 0;
}

/*
** =======================================================
** refresh
** =======================================================
*/
LCW_BOOLOK(wrefresh)
LCW_BOOLOK(wnoutrefresh)
LCW_BOOLOK(redrawwin)

static int lcw_wredrawln(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int beg_line = luaL_checkinteger(L, 2);
    int num_lines = luaL_checkinteger(L, 3);
    lua_pushboolean(L, B(wredrawln(w, beg_line, num_lines)));
    return 1;
}

LC_BOOLOK(doupdate)

/*
** =======================================================
** move
** =======================================================
*/

static int lcw_wmove(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkinteger(L, 2);
    int x = luaL_checkinteger(L, 3);
    lua_pushboolean(L, B(wmove(w, y, x)));
    return 1;
}

/*
** =======================================================
** scroll
** =======================================================
*/

static int lcw_wscrl(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int n = luaL_checkinteger(L, 2);
    lua_pushboolean(L, B(wscrl(w, n)));
    return 1;
}

/*
** =======================================================
** touch
** =======================================================
*/

static int lcw_touch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int changed;
    if (lua_isnoneornil(L, 2))
        changed = TRUE;
    else
        changed = lua_toboolean(L, 2);

    if (changed)
        lua_pushboolean(L, B(touchwin(w)));
    else
        lua_pushboolean(L, B(untouchwin(w)));
    return 1;
}

static int lcw_touchline(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkinteger(L, 2);
    int n = luaL_checkinteger(L, 3);
    int changed;
    if (lua_isnoneornil(L, 4))
        changed = TRUE;
    else
        changed = lua_toboolean(L, 4);
    lua_pushboolean(L, B(wtouchln(w, y, n, changed)));
    return 1;
}

static int lcw_is_linetouched(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int line = luaL_checkinteger(L, 2);
    lua_pushboolean(L, is_linetouched(w, line));
    return 1;
}

static int lcw_is_wintouched(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    lua_pushboolean(L, is_wintouched(w));
    return 1;
}

/*
** =======================================================
** getyx
** =======================================================
*/

static int lcw_getyx(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y, x;
    getyx(w, y, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, x);
    return 2;
}

static int lcw_getparyx(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y, x;
    getparyx(w, y, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, x);
    return 2;
}

static int lcw_getbegyx(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y, x;
    getbegyx(w, y, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, x);
    return 2;
}

static int lcw_getmaxyx(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y, x;
    getmaxyx(w, y, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, x);
    return 2;
}

/*
** =======================================================
** border
** =======================================================
*/

static int lcw_wborder(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);

    chtype ls = lc_opt_chtype(L, 2, 0);
    chtype rs = lc_opt_chtype(L, 3, 0);
    chtype ts = lc_opt_chtype(L, 4, 0);
    chtype bs = lc_opt_chtype(L, 5, 0);
    chtype tl = lc_opt_chtype(L, 6, 0);
    chtype tr = lc_opt_chtype(L, 7, 0);
    chtype bl = lc_opt_chtype(L, 8, 0);
    chtype br = lc_opt_chtype(L, 9, 0);

    lua_pushnumber(L, B(wborder(w, ls, rs, ts, bs, tl, tr, bl, br)));
    return 1;
}

static int lcw_box(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    chtype verch = lc_checkchtype(L, 2);
    chtype horch = lc_checkchtype(L, 3);

    lua_pushnumber(L, B(box(w, verch, horch)));
    return 1;
}

static int lcw_whline(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    chtype ch = lc_checkchtype(L, 2);
    int n = luaL_checkinteger(L, 3);

    lua_pushnumber(L, B(whline(w, ch, n)));
    return 1;
}

static int lcw_wvline(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    chtype ch = lc_checkchtype(L, 2);
    int n = luaL_checkinteger(L, 3);

    lua_pushnumber(L, B(wvline(w, ch, n)));
    return 1;
}


static int lcw_mvwhline(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkinteger(L, 2);
    int x = luaL_checkinteger(L, 3);
    chtype ch = lc_checkchtype(L, 4);
    int n = luaL_checkinteger(L, 5);

    lua_pushnumber(L, B(mvwhline(w, y, x, ch, n)));
    return 1;
}

static int lcw_mvwvline(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkinteger(L, 2);
    int x = luaL_checkinteger(L, 3);
    chtype ch = lc_checkchtype(L, 4);
    int n = luaL_checkinteger(L, 5);

    lua_pushnumber(L, B(mvwvline(w, y, x, ch, n)));
    return 1;
}

/*
** =======================================================
** clear
** =======================================================
*/

LCW_BOOLOK(werase)
LCW_BOOLOK(wclear)
LCW_BOOLOK(wclrtobot)
LCW_BOOLOK(wclrtoeol)

/*
** =======================================================
** slk
** =======================================================
*/
static int lc_slk_init(lua_State *L)
{
    int fmt = luaL_checkinteger(L, 1);
    lua_pushboolean(L, B(slk_init(fmt)));
    return 1;
}

static int lc_slk_set(lua_State *L)
{
    int labnum = luaL_checkinteger(L, 1);
    const char* label = luaL_checkstring(L, 2);
    int fmt = luaL_checkinteger(L, 3);

    lua_pushboolean(L, B(slk_set(labnum, label, fmt)));
    return 1;
}

LC_BOOLOK(slk_refresh)
LC_BOOLOK(slk_noutrefresh)

static int lc_slk_label(lua_State *L)
{
    int labnum = luaL_checkinteger(L, 1);
    lua_pushstring(L, slk_label(labnum));
    return 1;
}

LC_BOOLOK(slk_clear)
LC_BOOLOK(slk_restore)
LC_BOOLOK(slk_touch)

static int lc_slk_attron(lua_State *L)
{
    chtype attrs = lc_checkchtype(L, 1);
    lua_pushboolean(L, B(slk_attron(attrs)));
    return 1;
}

static int lc_slk_attroff(lua_State *L)
{
    chtype attrs = lc_checkchtype(L, 1);
    lua_pushboolean(L, B(slk_attroff(attrs)));
    return 1;
}

static int lc_slk_attrset(lua_State *L)
{
    chtype attrs = lc_checkchtype(L, 1);
    lua_pushboolean(L, B(slk_attrset(attrs)));
    return 1;
}

/*
** =======================================================
** addch
** =======================================================
*/

static int lcw_waddch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    chtype ch = lc_checkchtype(L, 2);
    lua_pushboolean(L, B(waddch(w, ch)));
    return 1;
}

static int lcw_mvwaddch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkinteger(L, 2);
    int x = luaL_checkinteger(L, 3);
    chtype ch = lc_checkchtype(L, 4);

    lua_pushboolean(L, B(mvwaddch(w, y, x, ch)));
    //lua_pushboolean(L, 1);
    return 1;
}

static int lcw_wechochar(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    chtype ch = lc_checkchtype(L, 2);

    lua_pushboolean(L, B(wechochar(w, ch)));
    return 1;
}

/*
** =======================================================
** addchstr
** =======================================================
*/

static int lcw_waddchnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int n = luaL_optinteger(L, 3, -1);
    chstr *cs = lc_checkchstr(L, 2);

    if (n < 0 || n > cs->len)
        n = cs->len;

    lua_pushboolean(L, B(wadd_wchnstr(w, cs->str, n)));
    return 1;
}

static int lcw_mvwaddchnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkinteger(L, 2);
    int x = luaL_checkinteger(L, 3);
    int n = luaL_optinteger(L, 5, -1);
    chstr *cs = lc_checkchstr(L, 4);

    if (n < 0 || n > cs->len)
        n = cs->len;

    lua_pushboolean(L, B(mvwadd_wchnstr(w, y, x, cs->str, n)));
    return 1;
}

/*
** =======================================================
** addstr
** =======================================================
*/

static int lcw_waddnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    size_t len;
    const char *str = luaL_checklstring(L, 2, &len);
    int n = luaL_optinteger(L, 3, -1);

    if (n < 0) n = len;

    lua_pushboolean(L, B(waddnstr(w, str, n)));
    return 1;
}

static int lcw_mvwaddnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkinteger(L, 2);
    int x = luaL_checkinteger(L, 3);
    size_t len;
    const char *str = luaL_checklstring(L, 4, &len);
    int n = luaL_optinteger(L, 5, -1);

    if (n < 0) n = len;

    lua_pushboolean(L, B(mvwaddnstr(w, y, x, str, n)));
    return 1;
}

/*
** =======================================================
** bkgd
** =======================================================
*/

static int lcw_wbkgdset(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    chtype ch = lc_checkchtype(L, 2);
    wbkgdset(w, ch);
    return 0;
}

static int lcw_wbkgd(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    chtype ch = lc_checkchtype(L, 2);
    lua_pushboolean(L, B(wbkgd(w, ch)));
    return 1;
}

static int lcw_getbkgd(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    lua_pushnumber(L, B(getbkgd(w)));
    return 1;
}

/*
** =======================================================
** inopts
** =======================================================
*/

static int lc_cbreak(lua_State *L)
{
    if (lua_isnoneornil(L, 1) || lua_toboolean(L, 1))
        lua_pushboolean(L, B(cbreak()));
    else
        lua_pushboolean(L, B(nocbreak()));
    return 1;
}

static int lc_echo(lua_State *L)
{
    if (lua_isnoneornil(L, 1) || lua_toboolean(L, 1))
        lua_pushboolean(L, B(echo()));
    else
        lua_pushboolean(L, B(noecho()));
    return 1;
}

static int lc_raw(lua_State *L)
{
    if (lua_isnoneornil(L, 1) || lua_toboolean(L, 1))
        lua_pushboolean(L, B(raw()));
    else
        lua_pushboolean(L, B(noraw()));
    return 1;
}

static int lc_halfdelay(lua_State *L)
{
    int tenths = luaL_checkinteger(L, 1);
    lua_pushboolean(L, B(halfdelay(tenths)));
    return 1;
}

static int lcw_intrflush(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int bf = lua_toboolean(L, 2);
    lua_pushboolean(L, B(intrflush(w, bf)));
    return 1;
}

static int lcw_keypad(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int bf = lua_isnoneornil(L, 2) ? 1 : lua_toboolean(L, 2);
    lua_pushboolean(L, B(keypad(w, bf)));
    return 1;
}

static int lcw_meta(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int bf = lua_toboolean(L, 2);
    lua_pushboolean(L, B(meta(w, bf)));
    return 1;
}

static int lcw_nodelay(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int bf = lua_toboolean(L, 2);
    lua_pushboolean(L, B(nodelay(w, bf)));
    return 1;
}

static int lcw_timeout(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int delay = luaL_checkinteger(L, 2);
    wtimeout(w, delay);
    return 0;
}

static int lcw_notimeout(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int bf = lua_toboolean(L, 2);
    lua_pushboolean(L, B(notimeout(w, bf)));
    return 1;
}

/*
** =======================================================
** outopts
** =======================================================
*/

static int lc_nl(lua_State *L)
{
    if (lua_isnoneornil(L, 1) || lua_toboolean(L, 1))
        lua_pushboolean(L, B(nl()));
    else
        lua_pushboolean(L, B(nonl()));
    return 1;
}

static int lcw_clearok(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    bool bf = lua_toboolean(L, 2);
    lua_pushboolean(L, B(clearok(w, bf)));
    return 1;
}

static int lcw_idlok(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    bool bf = lua_toboolean(L, 2);
    lua_pushboolean(L, B(idlok(w, bf)));
    return 1;
}

static int lcw_leaveok(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    bool bf = lua_toboolean(L, 2);
    lua_pushboolean(L, B(leaveok(w, bf)));
    return 1;
}

static int lcw_scrollok(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    bool bf = lua_toboolean(L, 2);
    lua_pushboolean(L, B(scrollok(w, bf)));
    return 1;
}

static int lcw_idcok(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    bool bf = lua_toboolean(L, 2);
    idcok(w, bf);
    return 0;
}

static int lcw_immedok(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    bool bf = lua_toboolean(L, 2);
    immedok(w, bf);
    return 0;
}

static int lcw_wsetscrreg(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int top = luaL_checkinteger(L, 2);
    int bot = luaL_checkinteger(L, 3);
    lua_pushboolean(L, B(wsetscrreg(w, top, bot)));
    return 1;
}

/*
** =======================================================
** overlay
** =======================================================
*/

static int lcw_overlay(lua_State *L)
{
    WINDOW *srcwin = lcw_check(L, 1);
    WINDOW *dstwin = lcw_check(L, 2);

    lua_pushboolean(L, B(overlay(srcwin, dstwin)));
    return 1;
}

static int lcw_overwrite(lua_State *L)
{
    WINDOW *srcwin = lcw_check(L, 1);
    WINDOW *dstwin = lcw_check(L, 2);

    lua_pushboolean(L, B(overwrite(srcwin, dstwin)));
    return 1;
}

static int lcw_copywin(lua_State *L)
{
    WINDOW *srcwin = lcw_check(L, 1);
    WINDOW *dstwin = lcw_check(L, 2);
    int sminrow = luaL_checkinteger(L, 3);
    int smincol = luaL_checkinteger(L, 4);
    int dminrow = luaL_checkinteger(L, 5);
    int dmincol = luaL_checkinteger(L, 6);
    int dmaxrow = luaL_checkinteger(L, 7);
    int dmaxcol = luaL_checkinteger(L, 8);
    int overlay = lua_toboolean(L, 9);

    lua_pushboolean(L, B(copywin(srcwin, dstwin, sminrow,
        smincol, dminrow, dmincol, dmaxrow, dmaxcol, overlay)));

    return 1;
}

/*
** =======================================================
** util
** =======================================================
*/

static int lc_unctrl(lua_State *L)
{
    chtype c = (chtype)luaL_checknumber(L, 1);
    lua_pushstring(L, unctrl(c));
    return 1;
}

static int lc_keyname(lua_State *L)
{
    int c = luaL_checkinteger(L, 1);
    lua_pushstring(L, keyname(c));
    return 1;
}

static int lc_delay_output(lua_State *L)
{
    int ms = luaL_checkinteger(L, 1);
    lua_pushboolean(L, B(delay_output(ms)));
    return 1;
}

static int lc_flushinp(lua_State *L)
{
    lua_pushboolean(L, B(flushinp()));
    return 1;
}

/*
** =======================================================
** delch
** =======================================================
*/

LCW_BOOLOK(wdelch)

static int lcw_mvwdelch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkinteger(L, 2);
    int x = luaL_checkinteger(L, 3);

    lua_pushboolean(L, B(mvwdelch(w, y, x)));
    return 1;
}

/*
** =======================================================
** deleteln
** =======================================================
*/

LCW_BOOLOK(wdeleteln)
LCW_BOOLOK(winsertln)

static int lcw_winsdelln(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int n = luaL_checkinteger(L, 2);
    lua_pushboolean(L, B(winsdelln(w, n)));
    return 1;
}

/*
** =======================================================
** getch
** =======================================================
*/

static int lcw_wgetch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int c = wgetch(w);

    if (c == ERR) return 0;

    lua_pushnumber(L, c);
    return 1;
}

static int lcw_mvwgetch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkinteger(L, 2);
    int x = luaL_checkinteger(L, 3);
    int c;

    if (wmove(w, y, x) == ERR) return 0;

    c = wgetch(w);

    if (c == ERR) return 0;

    lua_pushnumber(L, c);
    return 1;
}

static int lc_ungetch(lua_State *L)
{
    int c = luaL_checkinteger(L, 1);
    lua_pushboolean(L, B(ungetch(c)));
    return 1;
}

/*
** =======================================================
** getstr
** =======================================================
*/

static int lcw_wgetnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int n = luaL_optinteger(L, 2, 0);
    char buf[LUAL_BUFFERSIZE];

    if (n == 0 || n >= LUAL_BUFFERSIZE) n = LUAL_BUFFERSIZE - 1;
    if (wgetnstr(w, buf, n) == ERR)
        return 0;

    lua_pushstring(L, buf);
    return 1;
}

static int lcw_mvwgetnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkinteger(L, 2);
    int x = luaL_checkinteger(L, 3);
    int n = luaL_optinteger(L, 4, -1);
    char buf[LUAL_BUFFERSIZE];

    if (n == 0 || n >= LUAL_BUFFERSIZE) n = LUAL_BUFFERSIZE - 1;
    if (mvwgetnstr(w, y, x, buf, n) == ERR)
        return 0;

    lua_pushstring(L, buf);
    return 1;
}

/*
** =======================================================
** inch
** =======================================================
*/

static int lcw_winch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    lua_pushnumber(L, winch(w));
    return 1;
}

static int lcw_mvwinch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkinteger(L, 2);
    int x = luaL_checkinteger(L, 3);
    lua_pushnumber(L, mvwinch(w, y, x));
    return 1;
}

/*
** =======================================================
** inchstr
** =======================================================
*/

static int lcw_winchnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int n = luaL_checkinteger(L, 2);
    chstr *cs = chstr_new(L, n);

    if (wadd_wchnstr(w, cs->str, n) == ERR)
        return 0;

    return 1;
}

static int lcw_mvwinchnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkinteger(L, 2);
    int x = luaL_checkinteger(L, 3);
    int n = luaL_checkinteger(L, 4);
    chstr *cs = chstr_new(L, n);

    wchar_t *ws = malloc(sizeof(wchar_t) * n);
    for(int i = 0; i < cs->len; i++) {
      *ws++ = cs->str[i].chars[0];
    }

    if (mvwins_nwstr(w, y, x, ws, n) == ERR)
        return 0;

    return 1;
}

/*
** =======================================================
** instr
** =======================================================
*/

static int lcw_winnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int n = luaL_checkinteger(L, 2);
    char buf[LUAL_BUFFERSIZE];

    if (n >= LUAL_BUFFERSIZE) n = LUAL_BUFFERSIZE - 1;
    if (winnstr(w, buf, n) == ERR)
        return 0;

    lua_pushlstring(L, buf, n);
    return 1;
}

static int lcw_mvwinnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkinteger(L, 2);
    int x = luaL_checkinteger(L, 3);
    int n = luaL_checkinteger(L, 4);
    char buf[LUAL_BUFFERSIZE];

    if (n >= LUAL_BUFFERSIZE) n = LUAL_BUFFERSIZE - 1;
    if (mvwinnstr(w, y, x, buf, n) == ERR)
        return 0;

    lua_pushlstring(L, buf, n);
    return 1;
}

/*
** =======================================================
** insch
** =======================================================
*/

static int lcw_winsch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    cchar_t ch = lc_checkch(L, 2);
    lua_pushboolean(L, B(win_wch(w, &ch)));
    return 1;
}

static int lcw_mvwinsch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkinteger(L, 2);
    int x = luaL_checkinteger(L, 3);
    cchar_t ch = lc_checkch(L, 4);
    lua_pushboolean(L, B(mvwin_wch(w, y, x, &ch)));
    return 1;
}

/*
** =======================================================
** insstr
** =======================================================
*/

static int lcw_winsstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    size_t len;
    const char *str = luaL_checklstring(L, 2, &len);
    lua_pushboolean(L, B(winsnstr(w, str, len)));
    return 1;
}

static int lcw_mvwinsstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkinteger(L, 2);
    int x = luaL_checkinteger(L, 3);
    size_t len;
    const char *str = luaL_checklstring(L, 4, &len);
    lua_pushboolean(L, B(mvwinsnstr(w, y, x, str, len)));
    return 1;
}

static int lcw_winsnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    const char *str = luaL_checkstring(L, 2);
    int n = luaL_checkinteger(L, 3);
    lua_pushboolean(L, B(winsnstr(w, str, n)));
    return 1;
}

static int lcw_mvwinsnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkinteger(L, 2);
    int x = luaL_checkinteger(L, 3);
    const char *str = luaL_checkstring(L, 4);
    int n = luaL_checkinteger(L, 5);
    lua_pushboolean(L, B(mvwinsnstr(w, y, x, str, n)));
    return 1;
}

/*
** =======================================================
** pad
** =======================================================
*/

static int lc_newpad(lua_State *L)
{
    int nlines = luaL_checkinteger(L, 1);
    int ncols = luaL_checkinteger(L, 2);
    lcw_new(L, newpad(nlines, ncols));
    return 1;
}

static int lcw_subpad(lua_State *L)
{
    WINDOW *orig = lcw_check(L, 1);
    int nlines  = luaL_checkinteger(L, 2);
    int ncols   = luaL_checkinteger(L, 3);
    int begin_y = luaL_checkinteger(L, 4);
    int begin_x = luaL_checkinteger(L, 5);

    lcw_new(L, subpad(orig, nlines, ncols, begin_y, begin_x));
    return 1;
}

static int lcw_prefresh(lua_State *L)
{
    WINDOW *p = lcw_check(L, 1);
    int pminrow = luaL_checkinteger(L, 2);
    int pmincol = luaL_checkinteger(L, 3);
    int sminrow = luaL_checkinteger(L, 4);
    int smincol = luaL_checkinteger(L, 5);
    int smaxrow = luaL_checkinteger(L, 6);
    int smaxcol = luaL_checkinteger(L, 7);

    lua_pushboolean(L, B(prefresh(p, pminrow, pmincol,
        sminrow, smincol, smaxrow, smaxcol)));
    return 1;
}

static int lcw_pnoutrefresh(lua_State *L)
{
    WINDOW *p = lcw_check(L, 1);
    int pminrow = luaL_checkinteger(L, 2);
    int pmincol = luaL_checkinteger(L, 3);
    int sminrow = luaL_checkinteger(L, 4);
    int smincol = luaL_checkinteger(L, 5);
    int smaxrow = luaL_checkinteger(L, 6);
    int smaxcol = luaL_checkinteger(L, 7);

    lua_pushboolean(L, B(pnoutrefresh(p, pminrow, pmincol,
        sminrow, smincol, smaxrow, smaxcol)));
    return 1;
}


static int lcw_pechochar(lua_State *L)
{
    WINDOW *p = lcw_check(L, 1);
    cchar_t ch = lc_checkch(L, 2);

    lua_pushboolean(L, B(pecho_wchar(p, &ch)));
    return 1;
}

/*
** =======================================================
** attr
** =======================================================
*/

static int lcw_wattroff(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int attrs = luaL_checkinteger(L, 2);
    lua_pushboolean(L, B(wattroff(w, attrs)));
    return 1;
}

static int lcw_wattron(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int attrs = luaL_checkinteger(L, 2);
    lua_pushboolean(L, B(wattron(w, attrs)));
    return 1;
}

static int lcw_wattrset(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int attrs = luaL_checkinteger(L, 2);
    lua_pushboolean(L, B(wattrset(w, attrs)));
    return 1;
}

LCW_BOOLOK(wstandend)
LCW_BOOLOK(wstandout)


/*
** =======================================================
** text functions
** =======================================================
*/

#define LCT(name)                                   \
    static int lca_ ## name(lua_State* L)           \
    {                                               \
        const char *s = luaL_checkstring(L, 1);     \
        lua_pushboolean(L, name(*s));               \
        return 1;                                   \
    }

LCT(isalnum)
LCT(isalpha)
/*LCT(isblank)*/
LCT(iscntrl)
LCT(isdigit)
LCT(isgraph)
LCT(islower)
LCT(isprint)
LCT(ispunct)
LCT(isspace)
LCT(isupper)
LCT(isxdigit)

/*
** =======================================================
** panel functions
** =======================================================
*/
#if INCLUDEPANEL
#include "lpanel.c"
#endif

/*
** =======================================================
** register functions
** =======================================================
*/
/* chstr members */
static const luaL_Reg chstrlib[] =
{
    { "len",        chstr_len       },
    { "set_ch",     chstr_set_ch    },
    { "set_str",    chstr_set_str   },
    { "get",        chstr_get       },
    { "dup",        chstr_dup       },

    { NULL, NULL }
};

static const luaL_Reg windowlib[] =
{
    /* window */
    { "close", lcw_delwin  },
    { "sub", lcw_subwin },
    { "derive", lcw_derwin },
    { "move_window", lcw_mvwin },
    { "move_derived", lcw_mvderwin },
    { "clone", lcw_dupwin },
    { "syncup", lcw_wsyncup },
    { "syncdown", lcw_wsyncdown },
    { "syncok", lcw_syncok },
    { "cursyncup", lcw_wcursyncup },

    /* inopts */
    { "intrflush", lcw_intrflush },
    { "keypad", lcw_keypad },
    { "meta", lcw_meta },
    { "nodelay", lcw_nodelay },
    { "timeout", lcw_timeout },
    { "notimeout", lcw_notimeout },

    /* outopts */
    { "clearok", lcw_clearok },
    { "idlok", lcw_idlok },
    { "leaveok", lcw_leaveok },
    { "scrollok", lcw_scrollok },
    { "idcok", lcw_idcok },
    { "immedok", lcw_immedok },
    { "wsetscrreg", lcw_wsetscrreg },

    /* pad */
    { "subpad", lcw_subpad },
    { "prefresh", lcw_prefresh },
    { "pnoutrefresh", lcw_pnoutrefresh },
    { "pechochar", lcw_pechochar },

    /* move */
    { "move", lcw_wmove },

    /* scroll */
    { "scroll", lcw_wscrl },

    /* refresh */
    { "refresh", lcw_wrefresh },
    { "noutrefresh", lcw_wnoutrefresh },
    { "redraw", lcw_redrawwin },
    { "redraw_line", lcw_wredrawln },

    /* clear */
    { "erase", lcw_werase },
    { "clear", lcw_wclear },
    { "clear_to_bottom", lcw_wclrtobot },
    { "clear_to_eol", lcw_wclrtoeol },

    /* touch */
    { "touch", lcw_touch },
    { "touch_line", lcw_touchline },
    { "is_line_touched", lcw_is_linetouched },
    { "is_touched", lcw_is_wintouched },

    /* attrs */
    { "attroff", lcw_wattroff },
    { "attron", lcw_wattron },
    { "attrset", lcw_wattrset },
    { "standout", lcw_wstandout },
    { "standend", lcw_wstandend },

    /* getch */
    { "getch", lcw_wgetch },
    { "mvgetch", lcw_mvwgetch },

    /* getyx */
    { "getyx", lcw_getyx },
    { "getparyx", lcw_getparyx },
    { "getbegyx", lcw_getbegyx },
    { "getmaxyx", lcw_getmaxyx },

    /* border */
    { "border", lcw_wborder },
    { "box", lcw_box },
    { "hline", lcw_whline },
    { "vline", lcw_wvline },
    { "mvhline", lcw_mvwhline },
    { "mvvline", lcw_mvwvline },

    /* addch */
    { "addch", lcw_waddch },
    { "mvaddch", lcw_mvwaddch },
    { "echoch", lcw_wechochar },

    /* addchstr */
    { "addchstr", lcw_waddchnstr },
    { "mvaddchstr", lcw_mvwaddchnstr },

    /* addstr */
    { "addstr", lcw_waddnstr },
    { "mvaddstr", lcw_mvwaddnstr },

    /* bkgd */
    { "wbkgdset", lcw_wbkgdset },
    { "wbkgd", lcw_wbkgd },
    { "getbkgd", lcw_getbkgd },

    /* overlay */
    { "overlay", lcw_overlay },
    { "overwrite", lcw_overwrite },
    { "copy", lcw_copywin },

    /* delch */
    { "delch", lcw_wdelch },
    { "mvdelch", lcw_mvwdelch },

    /* deleteln */
    { "delete_line", lcw_wdeleteln },
    { "insert_line", lcw_winsertln },
    { "winsdelln", lcw_winsdelln },

    /* getstr */
    { "getstr", lcw_wgetnstr },
    { "mvgetstr", lcw_mvwgetnstr },

    /* inch */
    { "winch", lcw_winch },
    { "mvwinch", lcw_mvwinch },
    { "winchnstr", lcw_winchnstr },
    { "mvwinchnstr", lcw_mvwinchnstr },

    /* instr */
    { "winnstr", lcw_winnstr },
    { "mvwinnstr", lcw_mvwinnstr },

    /* insch */
    { "winsch", lcw_winsch },
    { "mvwinsch", lcw_mvwinsch },

    /* insstr */
    { "winsstr", lcw_winsstr },
    { "winsnstr", lcw_winsnstr },
    { "mvwinsstr", lcw_mvwinsstr },
    { "mvwinsnstr", lcw_mvwinsnstr },

    /* misc */
    {"__gc",        lcw_delwin  }, /* rough safety net */
    {"__tostring",  lcw_tostring},
    {NULL, NULL}
};

#define ECF(name) { #name, lc_ ## name },
#define ETF(name) { #name, lca_ ## name },
static const luaL_Reg curseslib[] =
{
    /* chstr helper function */
    { "new_chstr",      lc_new_chstr    },

    /* keyboard mapping */
    { "map_keyboard",   lc_map_keyboard },

    /* initscr */
    { "init",           lc_initscr      },
    { "done",           lc_endwin       },
    { "isdone",         lc_isendwin     },
    { "main_window",    lc_stdscr       },
    { "columns",        lc_COLS         },
    { "lines",          lc_LINES        },

    /* color */
    { "start_color",    lc_start_color  },
    { "has_colors",     lc_has_colors   },
    { "init_pair",      lc_init_pair    },
    { "pair_content",   lc_pair_content },
    { "colors",         lc_COLORS       },
    { "color_pairs",    lc_COLOR_PAIRS  },
    { "color_pair",     lc_COLOR_PAIR   },

    /* termattrs */
    { "baudrate",       lc_baudrate     },
    { "erase_char",     lc_erasechar    },
    { "kill_char",      lc_killchar     },
    { "has_insert_char",lc_has_ic       },
    { "has_insert_line",lc_has_il       },
    { "termattrs",      lc_termattrs    },
    { "termname",       lc_termname     },
    { "longname",       lc_longname     },

    /* kernel */
    { "ripoffline",     lc_ripoffline   },
    { "napms",          lc_napms        },
    { "cursor_set",     lc_curs_set     },

    /* beep */
    { "beep",           lc_beep         },
    { "flash",          lc_flash        },

    /* window */
    { "new_window",     lc_newwin       },

    /* pad */
    { "new_pad",        lc_newpad       },

    /* refresh */
    { "doupdate",       lc_doupdate     },

    /* inopts */
    { "cbreak",         lc_cbreak       },
    { "echo",           lc_echo         },
    { "raw",            lc_raw          },
    { "halfdelay",      lc_halfdelay    },

    /* util */
    { "unctrl",         lc_unctrl       },
    { "keyname",        lc_keyname      },
    { "delay_output",   lc_delay_output },
    { "flush_input",    lc_flushinp     },

    /* getch */
    { "ungetch",        lc_ungetch      },

    /* outopts */
    { "nl",             lc_nl           },

    /* slk */
    ECF(slk_init)
    ECF(slk_set)
    ECF(slk_refresh)
    ECF(slk_noutrefresh)
    ECF(slk_label)
    ECF(slk_clear)
    ECF(slk_restore)
    ECF(slk_touch)
    ECF(slk_attron)
    ECF(slk_attroff)
    ECF(slk_attrset)

    /* text functions */
    ETF(isalnum)
    ETF(isalpha)
    /*ETF(isblank)*/
    ETF(iscntrl)
    ETF(isdigit)
    ETF(isgraph)
    ETF(islower)
    ETF(isprint)
    ETF(ispunct)
    ETF(isspace)
    ETF(isupper)
    ETF(isxdigit)


    /* panel */

    {"new_panel",  lc_new_panel},
    {"update_panels",  lc_update_panels},
    {"bottom_panel",  lc_bottom_panel},
    {"top_panel",  lc_top_panel},

    /* terminator */
    {NULL, NULL}
};

int luaopen_lcurses(lua_State *L)
{

/*
** TODO: add upvalue table with lightuserdata keys and weak keyed
** values containing WINDOWS and PANELS used in above functions
*/

    /*
    ** create new metatable for window objects
    */
    luaL_newmetatable(L, PANELMETA);
    lua_pushvalue(L, -1);
    lua_setfield(L, -1, "__index");

    luaL_newlibtable(L, panellib);
    luaL_setfuncs(L, panellib, 1);

    /*
    ** create new metatable for window objects
    */
    luaL_newmetatable(L, WINDOWMETA);
    luaL_newlib(L, windowlib);
    lua_setfield(L, -2, "__index");

    /*
    ** create new metatable for chstr objects
    */
    luaL_newmetatable(L, CHSTRMETA);
    luaL_newlib(L, chstrlib);
    lua_setfield(L, -2, "__index");

    luaL_newlibtable(L, curseslib);
    lua_pushvalue(L, -1);
    luaL_setfuncs(L, curseslib, 1);

    return 1;
}
