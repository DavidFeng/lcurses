#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdio.h>

int main(int argc, char const* argv[])
{
  lua_State * L = luaL_newstate();
  luaL_openlibs(L);

  //luaL_dofile(L, "tl.lua");

  lua_close(L);
  return 0;
}
