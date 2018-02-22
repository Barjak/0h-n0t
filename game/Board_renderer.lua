require("Graphics")
require("Board")

function CLAMP(x, l, u)
   if x < l then
      return l
   elseif x > u then
      return u
   else
      return x
   end
end

function type2color(t)
   if NUMBER == t or FILLED == t then
      return style.FILLED_COLOR
   elseif EMPTY == t then
      return style.EMPTY_COLOR
   elseif WALL == t then
        return style.WALL_COLOR
   end
   print("BAD")
end


BoardRenderer = {}
BoardRenderer.__index = BoardRenderer
function BoardRenderer.new(board)
   if board == nil then
      return nil
   end
   local ret = {
      board = board,
      hint = nil,
      dirty_flag = 0,
      make_tile_function = TileCache,
      width = 100,
      height = 100,
      viewport_x = 0,
      viewport_y = 0,
      scale = 10.0,
      zoom = 1.0,
      -- ugly hack
      renderer_position_x = 0.0,
      renderer_position_y = 0.0

   }
   return setmetatable( ret, BoardRenderer)
end

function BoardRenderer:render(x, y)
   self.renderer_position_x = x
   self.renderer_position_y = y
   if nil == self.canvas or self:is_dirty() then
      self:prerender()
   end
   local oldmode, oldblendmode = lg.getBlendMode()
   lg.setBlendMode("alpha", "premultiplied")
   lg.draw( self.canvas, x, y)
   lg.setBlendMode(oldmode, oldblendmode)
end

function BoardRenderer:prerender()
   if nil == self.canvas then
      self.canvas = lg.newCanvas( self.width, self.height, "rgba8", 8)
      self:mark_dirty()
   end
   if not self:is_dirty() then
      return
   end

   local hint_x, hint_y = nil, nil
   if self.hint then
      self.hint.canvas = make_hint(self.hint.canvas, type2color(self.hint.type), self.scale * self.zoom)
      hint_x = self.hint.x
      hint_y = self.hint.y
   end
   self.canvas:renderTo(
      function()
         local make_tile = self.make_tile_function
         lg.clear()
         for index = 0,self.board.length-1 do
            local board_x = self.board:get_x(index)
            local board_y = self.board:get_y(index)
            if board_x >= -1 and board_x + 1 <= self.width and
               board_y >= -1 and board_y + 1 <= self.height then

               local viewport_x = (board_x - self.viewport_x) * self.scale * self.zoom
               local viewport_y = (board_y - self.viewport_y) * self.scale * self.zoom

               lg.setBlendMode("alpha", "premultiplied")
               local tile = self.board:get_tile(index)
               lg.draw(make_tile(type2color(tile.type),
                       tile.type == NUMBER and tile.value or nil, self.scale * self.zoom),
                       viewport_x, viewport_y)
               if board_x == hint_x and board_y == hint_y then
                  lg.draw(self.hint.canvas, viewport_x, viewport_y)
               end
               lg.setBlendMode("alpha")
            end
         end
      end
   )
   self:mark_clean()
end

-- returns whether it's dirty or not
function BoardRenderer:get_hint()
   local hint = self.board:get_hint()
   if nil == hint then
      return false
   end
   self.hint = {x = self.board:get_x(hint.id),
                y = self.board:get_y(hint.id),
                type = hint.type,
                canvas = nil}
   self:mark_dirty()
   return true
end
function BoardRenderer:clear_hint()
   if self.hint then
           -- print("hint")
      self.hint = nil
      self:mark_dirty()
      return true
   else
      return false
   end
end

function BoardRenderer:resize(width, height)
   local scale = math.min(width  / self.board.width,
                          height / self.board.height)
   local updated_width = scale * self.board.width
   local updated_height = scale * self.board.height
   if scale ~= self.scale or updated_width ~= self.width or updated_height ~= self.height then
      self.width = updated_width
      self.height = updated_height
      self.scale = scale
      self.canvas = nil
      self:mark_dirty()
   end
end
function BoardRenderer:win2board(x, y)
        return (x / (self.scale * self.zoom)) + self.viewport_x,
               (y / (self.scale * self.zoom)) + self.viewport_y
end

function BoardRenderer:click(x0, y0, button)
   local x, y = self:win2board(x0, y0)
   -- x, y = x - self.viewport_x, y - self.viewport_y
   local dirty = self.board:click(x, y, button)
   if dirty then
      self:mark_dirty()
      self:prerender()
   end
   return dirty
end

function BoardRenderer:move_wheel(zoom)
   local x, y = love.mouse.getPosition()
   x, y = x - self.renderer_position_x, y - self.renderer_position_y -- TODO: make this less ugly
   local new_zoom = CLAMP(self.zoom * (1 + 0.1 * zoom), 1.0, 4.0)
   local factor = new_zoom / self.zoom
   local scale = (self.scale * new_zoom)
   local move_x = (x - self.width / 2.0) / scale
   local move_y = (y - self.height / 2.0) / scale
   local dx = x / scale
   local dy = y / scale
   self.viewport_x = self.viewport_x + ((factor-1) * dx)
   self.viewport_y = self.viewport_y + ((factor-1) * dy)
   self.viewport_x = CLAMP(self.viewport_x + move_x * 0.08, 0.0, self.board.width - self.width / scale)
   self.viewport_y = CLAMP(self.viewport_y + move_y * 0.08, 0.0, self.board.height - self.height / scale)

   self.zoom = new_zoom
   self:mark_dirty()
   self:prerender()
end

function BoardRenderer:getWidth()
   return self.width
end
function BoardRenderer:getHeight()
   return self.height
end

function BoardRenderer:mark_dirty()
   self.dirty_flag = true
end
function BoardRenderer:mark_clean()
   self.dirty_flag = false
end
function BoardRenderer:is_dirty()
   return self.dirty_flag
end
