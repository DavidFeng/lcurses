--os.setlocale('en_US.UTF-8', 'all')
os.setlocale('', 'all')

package.cpath ='./?.so'

curses = require('lcurses')
local is = require 'inspect'

local function main()

  curses.slk_init(2)
  curses.init()
  local p = curses.new_panel(curses.main_window())
  curses.done()

  local mt = debug.getmetatable(p)
  print('panel meta is: ', is(mt))

  return print(is(curses))
end

local ok, r = xpcall(main, debug.traceback)
print(ok, r)
if true then
  return
end

curses.slk_init(2)

local function rip(w, columns)
  w:clear()
  w:mvaddstr(0,0,'世界')
  w:noutrefresh()
end

curses.ripoffline(true, rip)

local main = curses.init()

local function _main()
    curses.start_color()
    curses.nl(false)
    assert(main == curses.main_window())

    curses.doupdate()

    curses.echo(false)
    -- [[
    repeat
      c = main:getch()
      curses.napms(50)
    until (c)
    --]]

    -- clear the screen for terminals that don't restore the screen
    main:clear()
    main:noutrefresh()
    curses.slk_clear()
    curses.slk_noutrefresh()
    curses.doupdate()
end

local ok, msg = xpcall(_main, debug.traceback)
curses.done()

if not ok then
    print(msg)
end
