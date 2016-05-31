os.setlocale('', 'all')

local is = require 'inspect'

local curses = require('lcurses')

local function test()
  local win = curses.init()
  curses.echo(false)

  local str = 'hello 世界'
  win:mvaddstr(0, 0, str)
  win:mvaddstr(1, 0, 'line 2')
  local s = curses.new_chstr(8)
  local len = s:len()
  s:set_str(0, str)
  local r = win:mvaddchstr(3, 3, s)


  local ch = win:getch()
  return {
    len = len,
    r = r,
    ch = ch,
  }
end


-- [[
local ok, r = xpcall(test, debug.traceback)
curses.done()
if not ok then
  print(r)
else
  print(is(r))
end
--]]
