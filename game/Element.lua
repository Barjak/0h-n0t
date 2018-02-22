-- Do not simplify code by using more hash table lookup overhead
local max = math.max
local floor = math.floor
--- Element
Element = {}
Element.__index = Element
function Element.new( a)
   local ret = { child = a.child or {},
                 payload = a.payload,
                 vertical = a.vertical or false,
                 minH = a.minH or 0,
                 minW = a.minW or 0,
                 pad = a.pad or 0,
                 fun = a.fun}
   --- .w, .h, .majorPad,
   return setmetatable( ret, Element)
end
function Element.new_payload( p, f)
   return Element.new{ payload = p, fun = f}
end
local E = Element
E.pl = Element.new_payload
function E.hbox( a)
   return Element.new{ child = a, vertical = false}
end
function E.vbox( a)
   return Element.new{ child = a, vertical = true}
end
function E.free(self)
   -- if nil == self then
      -- return
   -- end
   for i,x in ipairs(self.child) do
           print(x)
      if x and x.free then
         x:free()
      end
   end
   if self.payload and self.payload.renderTo then
      self.payload:renderTo(function() lg.discard(true, true) end)
   end
end

--- A payload must be either a Love Drawable or a Lua Table with
---
--- 1. .w or .getWidth() field
--- 2. .h or .getHeight() field
--- 3. If it is a Table, a .render() field

function Element:add( e)
   table.insert( self.child, e)
end
function Element:collate( redo )
   if not redo and self.w and self.h then
      return self.w, self.h
   else
      self.w, self.h = 0, 0
      local n = 0

      if self.vertical == false then
         for i,c in pairs(self.child) do
            n = i
            a,b = c:collate( redo )
            self.w = self.w + a
            self.h = max( self.h, b)
         end
      elseif self.vertical == true then
         for i,c in pairs(self.child) do
            n = i
            a,b = c:collate( redo )
            self.w = max( self.w, a)
            self.h = self.h + b
         end
      end

      self.w = 2*self.pad + self.w
      self.h = 2*self.pad + self.h

      if self.payload then
         self.w = max( self.payload.w or self.payload:getWidth(), self.w)
         self.h = max( self.payload.h or self.payload:getHeight(), self.h)
      end

      self.majorPad = (self.vertical == false) and (self.minW - self.w) / (n+1) or (self.minH - self.h) / (n+1)
      self.majorPad = max( self.majorPad, 0)

      self.w = max( self.w, self.minW)
      self.h = max( self.h, self.minH)
      return self.w, self.h
   end
end
function Element:getHeight()
   if self.h then return self.h
   else self.w, self.h = self:collate() end
   return self.h
end
function Element:getWidth()
   if self.w then return self.w
   else self.w, self.h = self:collate() end
   return self.w
end
function Element:getPad()
   return self.pad or 0
end
function Element:render( x0, y0)
   local x, y = x0 or 0, y0 or 0
   if self.payload then
      x = x + (self:getWidth()-(self.payload.w or self.payload:getWidth()))/2
      y = y + (self:getHeight()-(self.payload.h or self.payload:getHeight()))/2
      if type(self.payload) == "table" then
         self.payload:render( floor(x+0.5), floor(y+0.5))
         return
      end
      do
         local oldmode, oldblendmode = lg.getBlendMode()
         lg.setBlendMode("alpha", "premultiplied")
         lg.draw(self.payload, floor(x+0.5), floor(y+0.5))
         lg.setBlendMode(oldmode, oldblendmode)
      end
   end
   ---
   if not self.majorPad then self:collate() end
   if self.vertical == false then
      x = x + self.pad + self.majorPad
      for _,c in pairs(self.child) do
         c:render( x, y + (self:getHeight() - c:getHeight()) / 2 )
         x = x + c:getWidth() + self.majorPad
      end
   elseif self.vertical == true then
      y = y + self.pad + self.majorPad
      for _,c in pairs(self.child) do
         c:render( x + (self:getWidth() - c:getWidth()) / 2 , y )
         y = y + c:getHeight() + self.majorPad
      end
   end
   ---
end

function Element:_search( x0, y0)
   local x, y = x0 or 0, y0 or 0
   if x < self:getWidth() and y < self:getHeight()
   and x > 0 and y > 0 then
      if self.fun then return self.fun, x, y end
      if not self.majorPad then self:collate() end
      if self.vertical == false then
         x = self.pad + self.majorPad
         for _,c in pairs(self.child) do
            local fun,x_ret,y_ret = c:_search( x0 - x,
                                               y0 - (self:getHeight() - c:getHeight()) / 2)
            if fun then return fun,x_ret,y_ret end
            x = x + c:getWidth() + self.majorPad
         end
      elseif self.vertical == true then
         y = self.pad + self.majorPad
         for _,c in pairs(self.child) do
            local fun,x_ret,y_ret = c:_search( x0 - (self:getWidth() - c:getWidth()) / 2,
                                               y0 - y)
            if fun then return fun,x_ret,y_ret end
            y = y + c:getHeight() + self.majorPad
         end
      end
   end
   return nil
end
function Element:search( x0, y0, button)
   local result,x,y = self:_search( x0, y0)
   if type(result) == "function"
   then
      return
         function()
            result( x, y, button)
         end
   end
   return function() end
end
