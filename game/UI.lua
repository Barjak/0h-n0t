require("Board")
require("Board_renderer")
require("Graphics")
require("Element")
local E = Element
local sin = math.sin
local floor, min = math.floor, math.min
local lg = require("love.graphics")
--- UI Logic

local h1 = nil
local h2 = nil

local UI = {}
UI.__index = UI

function UI.create()
   local ret = setmetatable(
      {
         renderFun = function() end,
         main = nil,               -- main: the currently rendering root element
         dirty_flag = true,         -- dirty_flag: should .main be reshaped?
         -- TileCache = tile_cache(),
         thread = nil,             -- thread: board creation happens here
         board = nil,              -- board: the board structure itself
         -- Dynamic elements
         board_renderer = nil,
         progress = E.pl(nil),
         loading_callback = nil,
         timer = E.pl(nil),
         timer_callback = nil,
         hint_indicator = E.pl(nil),
         hint_indicator_callback = nil,
         victory = E.pl(nil),
         n_seconds = -1,

         width = nil, height = nil,-- width & height of the window
      },
      {__index = UI})
   ret.board, ret.n_seconds = Board.load()
   ret.board_renderer = BoardRenderer.new(ret.board)
   if ret.board ~= nil then
      ret:launch_updaters()
   end
   return ret
end

function UI:is_dirty() return self.dirty_flag end
function UI:mark_dirty() self.dirty_flag = true end
function UI:mark_clean() self.dirty_flag = false end

function UI:refresh_attributes()
   self.width, self.height = love.window.getMode()
end

function UI:resize()
   self:mark_dirty()
   lg.setBackgroundColor( _BACKGROUND_COLOR_)

   local desiredwidth = 320
   local desiredheight = 480
   local aspectRatio = desiredwidth / desiredheight
   local sizeToWidth = (self.width / self.height) < aspectRatio
   style["box-width"] = floor( sizeToWidth and self.width or (self.height/desiredheight) * desiredwidth)
   style["box-height"] = floor( sizeToWidth and (self.width/desiredwidth) * desiredheight or self.height)
   local containerSize = style["box-width"]
   -- if self.board and self.board.width and self.board.height then
      -- style["tile-size"] = floor( min( (containerSize / self.board.width),
                                     -- (style["box-height"] / self.board.height)))
   -- end
   style["menu-option-font-size"] =  floor( containerSize * 0.1)
   style["menu-option-line-height"] = floor( containerSize * 0.1)
   style["menu-option-vertical-padding"] = floor( containerSize * 0.10)-- 0.05
   style["h1-font-size"] = floor( containerSize * 0.24)
   style["h2-font-size"] = floor( containerSize * 0.18)
   style["option-font-size"] =  floor( containerSize * 0.07)

   style["header-height"] = floor( style["box-height"] * 0.10)
   style["timer-height"] = floor( style["box-height"] * 0.05)
   style["button-height"] = floor( style["box-height"] * 0.08)

   style["icon-size"] = floor( (24/320) * containerSize)
   style["score-size"] = floor( containerSize * 0.1)

   h1 = _FONT_("Molle-Regular.ttf", style["h1-font-size"])
   h2 = _FONT_("Molle-Regular.ttf", style["h2-font-size"])

   if self.board_renderer then
      self.board_renderer:resize(self.width, style["box-height"] - style["header-height"] - style["timer-height"] - style["button-height"])
   end
   lg.clear( _BACKGROUND_COLOR_)
   self.main = nil
end

function UI:set_context(command)
   self.renderFun = command
   self.main = nil
   self:mark_dirty()
   return true
end

function UI:go_menu()    return self:set_context(UI.draw_menu)   end
function UI:go_select()  return self:set_context(UI.draw_select)  end
function UI:go_about()   return self:set_context(UI.draw_about)   end
function UI:go_rules()   return self:set_context(UI.draw_rules)   end
function UI:go_loading() return self:set_context(UI.draw_loading) end
function UI:goGame()
   if self.thread then
      return self:go_loading()
   end
   return self:set_context(UI.draw_game)
end

function UI:move_wheel(zoom)
   if self.board_renderer ~= nil and self.renderFun == UI.draw_game then
      self.board_renderer:move_wheel(zoom)
      self:mark_dirty()
   end
end

function UI:new_game(width, height)
   self.thread = love.thread.newThread("Board_create_async.lua")
   local chout = love.thread.getChannel("to_thread")
   while chout:pop() do end
   local chin = love.thread.getChannel("from_thread")
   while chin:pop() do end

   self:go_loading()
   self.board = nil
   self.board_renderer = nil

   chout:push({ width = width,
                height = height or width,
                max_number = math.min(width, 9),
                difficulty = HARD} )
   self.thread:start()

   self.loading_callback = function(dt, this_fun)
      if nil == self.thread then
         -- print("removed update function")
         self.stop_loading_callback()
         return
      end
      local data = chin:pop()
      if data then
         if data.deserialize then
            self.board = Board.deserialize(data)
            self.board_renderer = BoardRenderer.new(self.board)
            self.n_seconds = -1
            self.thread = nil
            self.victory.payload = nil
            self:launch_updaters()
            self:goGame()
            self:resize()
         elseif data.completion then
            self.progress.payload = progress_indicator(self.progress.payload, 0.5 * style["box-width"], data.completion, style.FILLED_COLOR)
            self:mark_dirty()
         elseif data.killed then
            -- print("Thread killed")
         end
      end
   end
   self.stop_loading_callback = function(dt, this_fun)
         RemoveUpdateFun(self.loading_callback)
         self.loading_callback = nil
         self.stop_loading_callback = nil
         chout:push({kill = true})
   end

   AddUpdateFun(self.loading_callback)
end

function UI:launch_updaters()
   local my_board = self.board
   -- Check if board is solved
   self.check_solve_callback =
   function(dt, this_fun)
      if not self.board or self.board ~= my_board then
         RemoveUpdateFun(this_fun)
         RemoveUpdateFun(self.timer_callback)
         RemoveUpdateFun(self.check_solve_callback)
         return
      end
      if self.board:is_solved() then
         self.victory.payload = _FONT_("JosefinSans-Bold.ttf", style["header-height"] * 0.8)("Victory!", GRAY)
         self:mark_dirty()
         RemoveUpdateFun(this_fun)
         RemoveUpdateFun(self.timer_callback)
         RemoveUpdateFun(self.check_solve_callback)
      end
   end

   -- Update timer
   local old_time = love.timer.getTime()
   self.timer_callback =
   function(dt, this_fun)
      local new_time = floor(love.timer.getTime())
      if new_time ~= old_time and self.renderFun == UI.draw_game and not self.board:is_solved() then
         self.n_seconds = self.n_seconds + 1
         self.timer.payload = timer(self.timer.payload, 300, style["timer-height"], self.n_seconds)
         self:mark_dirty()
         old_time = new_time
      end
   end

   -- Check for mistakes
   local old_mistake = self.board:get_mistake()
   local time = 0.0
   self.hint_indicator_callback =
   function(dt, this_fun)
      if self.board then
         local new_mistake = self.board:get_mistake()
         if -1 == new_mistake then
            if -1 ~= old_mistake then
               self.hint_indicator.payload = _IMG_("img/eye.png")(style["icon-size"], style["icon-opacity"])
               self:mark_dirty()
               time = 0
            end
         else
            self.hint_indicator.payload = _IMG_("img/eye.png")(style["icon-size"],
                                                               style["icon-opacity"] * (0.5 * (1 + sin(2 * time))) )
            self:mark_dirty()
            time = time + dt
         end
         old_mistake = new_mistake
      end
   end
   AddUpdateFun(self.hint_indicator_callback)
   AddUpdateFun(self.timer_callback)
   AddUpdateFun(self.check_solve_callback)
end

function UI:draw_select()
   if not self.main then
      local scorefont = _FONT_("JosefinSans-Bold.ttf", style["score-size"])

      local Select_a_size = Element.new{ payload = h2( "Select a size", style.GRAY)}

      local dot_box_width = style["box-width"] * 0.60
      local tile_size = min(dot_box_width / 3.0)
      local dot_box = E.new{minW = dot_box_width,
                            vertical = true,
                            child = {
                               E.hbox{
                                  E.pl(make_tile(type2color(FILLED), 8, tile_size, 0.3),
                                       function() self:new_game(8) end),
                                  E.pl(make_tile(type2color(WALL), 12, tile_size, 0.3),
                                       function() self:new_game(12) end),
                                  E.pl(make_tile(type2color(FILLED), 16, tile_size, 0.3),
                                       function() self:new_game(16) end)
                               },
                               E.hbox{
                                  E.pl(make_tile(type2color(WALL), 32, tile_size, 0.3),
                                       function() self:new_game(32) end),
                                  E.pl(make_tile(type2color(FILLED), 64, tile_size, 0.3),
                                       function() self:new_game(64) end),
                                  E.pl(make_tile(type2color(WALL), 128, tile_size, 0.3),
                                       function() self:new_game(128) end)

                               },
                               E.hbox{
                                  E.pl(make_tile(type2color(FILLED), "40x20", tile_size, 0.3),
                                       function() self:new_game(40,20) end),
                                  E.pl(make_tile(type2color(WALL), "80x40", tile_size, 0.3),
                                       function() self:new_game(80,40) end),
                                  E.pl(make_tile(type2color(FILLED), "160x80", tile_size, 0.3),
                                       function() self:new_game(160,80) end)
                               }}}
      local buttons = E.hbox{E.pl(_IMG_("img/close.png")(style["icon-size"], 0.5),
                                  function()
                                     self:go_menu()
                                  end),
                             self.board and E.pl(_IMG_("img/play.png")(style["icon-size"], 0.5),
                                                 function() self:goGame() end)
                                        or nil}
      local container = E.new{child = {Select_a_size,
                                       dot_box,
                                       E.pl(scorefont(" ", {0,0,0,128})),
                                       buttons},
                              vertical = true,
                              minH = style["box-height"]}
      self.main = E.new{child = {container},
                        minW  = self.width}
   else
      self.main:collate(true)
   end
   self.main:render()

   self:mark_clean()
end

function UI:draw_menu()
   if not self.main then
      local font = _FONT_("JosefinSans-Bold.ttf", style["menu-option-font-size"])

      local continue = (self.board ~= nil)

      local options  = E.new{ child = {
                                 E.pl(h1("Oh not", GRAY)),
                                 E.pl(font("Continue", continue and GRAY or LIGHT_GRAY),
                                                       continue and function() self:goGame()   end),
                                 E.pl(font("Play", GRAY),
                                      function() self:go_select()   end),
                                 E.pl(font("About", GRAY),
                                      function() self:go_about() end),
                                 E.pl(font("Exit", GRAY),
                                      function()
                                         if self.board and not self.board:is_solved() then
                                            self.board:save(self.n_seconds)
                                         end
                                         love.event.quit()
                                      end) },
                              vertical = true,
                              minH = style["box-height"] * 0.8 }
      self.main      = E.new{ child = {options},
                              minW = self.width }
   else
      self.main:collate(true)
   end
   self.main:render()

   self:mark_clean()
end

function UI:draw_game()
   local iconSize, iconOpacity = style["icon-size"], style["icon-opacity"]

   if not self.main then
      self.victory = E.new{payload = nil, minH = style["header-height"]}
      local board  = E.pl(self.board_renderer,
                          function(x,y,button)
                              local dirty = self.board_renderer:click(x,y,button)
                              local dirty2 = self.board_renderer:clear_hint()
                              if dirty or dirty2 then
                                 self:mark_dirty()
                              end
                          end)
      self.timer = E.new{payload = self.timer.payload, minH = style["timer-height"]}
      self.hint_indicator = E.pl(_IMG_("img/eye.png")(style["icon-size"], style["icon-opacity"]),
                                 function ()
                                    local dirty = self.board_renderer:get_hint()
                                    if dirty then
                                       self:mark_dirty()
                                    end
                                 end)
      local buttons = E.new{child =
                               {E.pl(_IMG_("img/close.png")(iconSize, iconOpacity),
                                     function()
                                        self:go_menu()
                                     end
                                    ),
                                E.pl(_IMG_("img/history.png")(iconSize, iconOpacity),
                                     function()
                                        self.board:undo()
                                        self.board_renderer:mark_dirty()
                                        self:mark_dirty()
                                     end),
                                self.hint_indicator,
                                -- E.pl(_IMG_("img/cog.png")(iconSize, iconOpacity)),
                                -- E.pl(_IMG_("img/question.png")(iconSize, iconOpacity),
                                      -- function()
                                              -- return nil
                                      -- end )
                                   }, minW = style["box-width"]}

      local box = E.new{child = {self.victory, board, self.timer, buttons}, vertical = true }
      self.main = E.new{child = {box}, minW = self.width, minH = self.height}
   else
      self.main:collate(true)
   end
   self.main:render()
   self:mark_clean()
end

function UI:draw_about()
   if not self.main then
      local font = _FONT_("JosefinSans-Bold.ttf", 0.5 * style["menu-option-font-size"])
      local box = E.new{child = {E.pl(h2("About", GRAY)),
                                 E.pl(font(" ")),

                                 E.pl(font("This is a clone of Martin Kool's")),
                                 E.pl(font("wonderful game 0h n0.")),
                                 E.pl(font("I wanted to play on a very large")),
                                 E.pl(font("board, but the original implentation")),
                                 E.pl(font("couldn't generate large boards quickly.")),
                                 E.pl(font(" ")),
                                 E.pl(font("This version uses a toy constraint")),
                                 E.pl(font("solver to generate very large boards.")),
                                 E.pl(font(" ")),
                                 E.pl(font("However, it's much uglier and less")),
                                 E.pl(font("polished than Martin's original")),
                                 E.pl(font("Javascript app.")),
                                 E.pl(font(" ")),
                                 E.pl(font("Play the original at 0hn0.com")),


                                 E.new{payload = _IMG_("img/play.png")(style["icon-size"], style["icon-opacity"]),
                                       fun = function()
                                          self:go_rules()
                                       end,
                                       pad = style["button-height"]}},
                        vertical = true }
      self.main = E.new{child = {box}, minW = self.width, minH = self.height}
   else
      self.main:collate(true)
   end
   self.main:render()
   self:mark_clean()
end
function UI:draw_rules()
   if not self.main then
      local font = _FONT_("JosefinSans-Bold.ttf", 0.5 * style["menu-option-font-size"])
      local box = E.new{child = {E.pl(h2("Rules", GRAY)),
                                 E.pl(font(" ")),
                                 E.pl(font("Blue dots can see others")),
                                 E.pl(font("in their own row and column.")),
                                 E.pl(font(" ")),
                                 E.pl(font("Their numbers tell how many.")),
                                 E.pl(font(" ")),
                                 E.pl(font("Red dots block their view.")),
                                 E.pl(font(" ")),
                                 E.pl(font("Your progress will be saved.")),
                                 E.pl(font(" ")),
                                 E.pl(font("Use the mousewheel to zoom.")),

                                 E.new{payload = _IMG_("img/close.png")(style["icon-size"], style["icon-opacity"]),
                                       fun = function()
                                          self:go_menu()
                                       end,
                                       pad = style["button-height"]}},
                        vertical = true }
      self.main = E.new{child = {box}, minW = self.width, minH = self.height}
   else
      self.main:collate(true)
   end
   self.main:render()
   self:mark_clean()
end
function UI:draw_loading()
   if not self.main then
      self.progress = E.pl(nil)
      self.main = E.new{minW = self.width,
                        child = {
                           E.new{minH = style["box-width"], vertical = true,
                                 child = {E.pl(h2("loading", GRAY)),
                                          self.progress,
                                          E.pl(_IMG_("img/close.png")(style["icon-size"], style["icon-opacity"]),
                                               function()
                                                  self:go_menu()
                                                  if self.stop_loading_callback ~= nil then
                                                     self.stop_loading_callback()
                                                  end
                                               end),
      }}}}
   else
      self.main:collate(true)
   end
   self.main:render()
   self:mark_clean()
end

return UI
