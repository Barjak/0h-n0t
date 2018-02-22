require("math")
require("Board")
function main()
   local input = love.thread.getChannel("to_thread")
   local job_data = input:pop()

   local width, height, maxNumber = job_data.width, job_data.height, job_data.maxNumber
   if nil == height or 0 == height then
      height = width
   end

   local b = Board(width, height)
   b:initialize(maxNumber)

   local out = love.thread.getChannel("from_thread")
   local batch_size = math.sqrt(width*height) * 2
   -- local t0 = os.clock()
   local completion = 0.0
   while completion < 1.0 do
      local signal = input:pop()
      if signal and signal.kill then
         out:push({killed = true})
         return
      end
      completion = b:reduce(batch_size)
      out:push({completion = completion})
   end
   -- local dt = os.clock() - t0
   -- print("seconds: " .. dt .. " seconds per tile: " .. dt/(width*height))

   out:push(b:serialize())
end

main()
