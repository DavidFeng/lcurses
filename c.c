#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdio.h>
#include <wchar.h>
#include <locale.h>

int main(int argc, char const* argv[])
{

  setlocale(LC_ALL, "");

  lua_State * L = luaL_newstate();
  luaL_openlibs(L);

  // stack empty

  int n = lua_gettop(L);

  lua_getglobal(L, "utf8");
  lua_getfield(L, -1, "codes");
  lua_remove(L, -2); // remove global utf8
  lua_pushstring(L, "hello 你好世界");
  lua_call(L, 1, LUA_MULTRET);

  while (1) {
    lua_pushvalue(L, -3);
    lua_insert(L, -3); // f f s v
    lua_pushvalue(L, -2); // f f s v s
    lua_insert(L, -4); // f s f s v

    lua_call(L, 2, LUA_MULTRET);
    if (lua_gettop(L) == n + 2) {
      lua_pop(L, 2);
      break;
    } else {
      int c = lua_tointeger(L, -1);
      printf("%d: %lc\n", c, c);
      lua_remove(L, -1);
    }
  }

  lua_close(L);
  return 0;
}
