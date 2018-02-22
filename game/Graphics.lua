lg = require("love.graphics")
local lg = lg
local util = require("util")
local floor = math.floor
local max = math.max
local cos = math.cos
local sin = math.sin

GRAY = {51,51,51,255}
LIGHT_GRAY = {150,150,150,255}
_BLACK_ = {0,0,0,255}
_WHITE_ = {255, 255, 255, 255}
_BACKGROUND_COLOR_ = _WHITE_

style = {}
style["TILE_PADDING"] = 0.92
style["icon-opacity"] = 0.4

style.WALL_COLOR = {255,  56,  75, 255}
style.FILLED_COLOR = { 28, 192, 224, 255}
style.EMPTY_COLOR = {238, 238, 238, 255}

_FONT_ = util.Cache.new{
   method =
      function(name, size)
         local font = lg.newFont( name, size)
         return
            function( str, color)
               local txt = lg.newText( font)
               txt:setf({color or _BLACK_, str}, 10000, "left")
               return txt
            end
      end,
   key =
      function(name, size)
         return tostring(size) .. name
      end,
   size = 16}
_IMG_ = util.Cache.new{
   method =
      function(name)
         local img = lg.newImage( name)
         return
            function( size, opacity)
               opacity = opacity or 255
               size = size or 1
               local wScale, hScale = size / img:getWidth(), size / img:getHeight()
               local renderer = {
                  render = function( self,x,y)
                     local r,g,b,a = lg.getColor()
                     lg.setColor( 0,0,0, floor( opacity * a))
                     lg.draw( img, x, y, 0, wScale, hScale)
                     lg.setColor( r,g,b,a)
                  end,
                  w = img:getWidth() * wScale,
                  h = img:getHeight() * hScale
               }
               return renderer
            end
      end,
   key =
      function(name)
         return name
      end,
   size = 32}

function make_tile(color, number, scale_f, text_scale, segments)
   local scale = floor(scale_f)
   segments = segments or 50
   text_scale = text_scale or 0.75
   local radius = scale * 0.5 * style.TILE_PADDING
   local tile = lg.newCanvas( scale, scale, "rgba8", 16 )
   tile:setFilter("linear", "linear", 2)
   tile:renderTo( function()
         lg.setColor( color )
         lg.circle("fill", scale/2, scale/2, radius, segments)
         lg.setColor( _WHITE_ )
         if number then
            local font = _FONT_("JosefinSans-Bold.ttf", floor( scale * text_scale))
            txt = font( tostring( number), _WHITE_)
            do
               local oldmode, oldblendmode = lg.getBlendMode()
               lg.setBlendMode("alpha")
               lg.draw( txt, scale/2, scale/2, nil, nil, nil,
                        txt:getWidth()/2, txt:getHeight()/2)
               lg.setBlendMode(oldmode, oldblendmode)
            end
         end
   end)
   return tile
end

function make_hint(old_canvas, color, scale, segments)
   segments = segments or 50
   if nil == canvas or canvas:getWidth() ~= scale or canvas:getHeight() ~= scale then
      canvas = lg.newCanvas(scale, scale, "rgba8", 16)
   end
   canvas:setFilter("linear", "linear", 2)
   canvas:renderTo(function()
           lg.clear({0,0,0,0})
           lg.setColor(color)
           lg.setLineWidth(max(3, scale / 10))
           lg.circle("line", scale/2, scale/2, scale/2 * .85, segments)
           lg.setColor(_WHITE_)
   end)
   return canvas
end

TileCache = util.Cache.new{
   method = function(color, number, scale)
               return make_tile(color, number, scale)
            end,
   key = function( color, number, scale)
            return tostring(color) .. tostring(number) .. tostring(scale)
         end,
   size = 25
}

function progress_indicator(old_canvas, diameter, fraction, color)
   local radius = floor(diameter / 2.0)
   local canvas = old_canvas
   local n_segments = 60
   local k_segments = fraction * n_segments

   if nil == canvas or canvas:getWidth() ~= diameter or canvas:getHeight() ~= diameter then
      canvas = lg.newCanvas(diameter, diameter, "rgba8", 16)
   end
   local twoPI = 2 * 3.14159265358
   local vertices = {radius,radius}
   local i = 2
   for x = 0,k_segments do
      vertices[i+1] = radius * (1 + cos(twoPI * x/n_segments - twoPI/4))
      vertices[i+2] = radius * (1 + sin(twoPI * x/n_segments - twoPI/4))
      i = i + 2
   end
   vertices[i+1] = radius * (1 + cos(twoPI * fraction - twoPI/4))
   vertices[i+2] = radius * (1 + sin(twoPI * fraction - twoPI/4))
   i = i + 2
   if (i < 6) then
      return
   end
   canvas:setFilter("linear", "linear", 2)
   canvas:renderTo(function()
      lg.clear(_WHITE_)
      lg.setColor(color)
      lg.polygon("fill", vertices)
      lg.setColor(_WHITE_)
   end)
   return canvas
end

function timer(old_canvas, width, height, seconds)
   width = floor(width)
   height = floor(height)
   local canvas = old_canvas
   if nil == canvas or canvas:getWidth() ~= width or canvas:getHeight() ~= height then
      canvas = lg.newCanvas(width, height, "rgba8", 16)
   end
   canvas:renderTo(function()
      lg.clear(_WHITE_)
      local font = _FONT_("JosefinSans-Bold.ttf", height)
      local minutes = floor(seconds / 60)
      seconds = seconds % 60
      local txt = nil
      if minutes > 0 then
         txt = font(string.format("%d:%02d", minutes, seconds), _BLACK_)
      else
         txt = font(string.format("%d", seconds), _BLACK_)
      end
      do
         local oldmode, oldblendmode = lg.getBlendMode()
         lg.setBlendMode("alpha")
         lg.draw(txt, width/2, height/2, nil, nil, nil,
                 txt:getWidth()/2, txt:getHeight()/2)
         lg.setBlendMode(oldmode, oldblendmode)
      end
   end)
   return canvas
end
