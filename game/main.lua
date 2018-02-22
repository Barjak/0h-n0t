#!/bin/sh

local UI = require("UI")

local update_table = {}
function AddUpdateFun(fun)
   update_table[fun] = fun
end
function RemoveUpdateFun(fun)
   update_table[fun] = nil
end

love.window.setTitle("0h n0")

local Game = UI.create()
Game:go_menu()

--- Engine Callbacks
love.resize = function( w, h)
   Game.width, Game.height = w, h
   Game:resize()
end

love.focus = function( it_is)
   if it_is then
      Game:mark_dirty()
   end
end

love.load = function()
   love.window.setMode(1000, 1000, {resizable = true, minwidth = 320, minheight = 480})
   if love.window.isCreated() then
      Game:refresh_attributes()
      Game:resize()
   else print("Window not created")
   end
end

love.draw = function()
   Game:renderFun()
end

love.mousepressed = function(x, y, button, istouch)
   Game.main:search(x,y,button)()
end
love.wheelmoved = function(x, y)
   Game:move_wheel(y)
end

local collect = 0
love.update = function(dt)
   -- print(collectgarbage("collect"))
   for k,fun in pairs(update_table) do
      fun(dt, fun)
   end

   collect = collect + 1
   if collect == 100 then
      collectgarbage()
      collect = 0
   end
end

-----------
--- modified from love/src/scripts/boot.lua
-----------
function love.run()
   if love.math then
      love.math.setRandomSeed(os.time())
   end

   if love.load then love.load(arg) end

   -- We don't want the first frame's dt to include time taken by love.load.
   if love.timer then love.timer.step() end

   local dt = 0

   -- Main loop time.
   while true do
      -- Process events.
      if love.event then
         love.event.pump()
         for name, a,b,c,d,e,f in love.event.poll() do
            if name == "quit" then
               if not love.quit or not love.quit() then
                  return a
               end
            end
            love.handlers[name](a,b,c,d,e,f)
         end
      end

      -- Update dt, as we'll be passing it to update
      if love.timer then
         love.timer.step()
         dt = love.timer.getDelta()
      end

      -- Call update and draw
      if love.update then love.update(dt) end -- will pass 0 if love.timer is disabled

      if Game:is_dirty() and love.graphics and love.graphics.isActive() then -- changed
         love.graphics.clear(love.graphics.getBackgroundColor())
         love.graphics.origin()
         if love.draw then love.draw() end
         love.graphics.present()
      end

      if love.timer then love.timer.sleep(0.01) end -- changed
   end

end
