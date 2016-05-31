os.setlocale('', 'all')

curses = require('lcurses')
local is = require 'inspect'

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
    main:getch()

    -- clear the screen for terminals that don't restore the screen
    main:clear()
    main:noutrefresh()
    curses.slk_clear()
    curses.slk_noutrefresh()
    curses.doupdate()
end

local ok, msg = xpcall(_main, debug.traceback)
curses.done()
if not ok then print(msg) end
