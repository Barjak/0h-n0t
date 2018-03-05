local C = require("ffi")
assert(C)
Board = {}
Board.__index = Board

local header = io.open("Board.h", "r")
C.cdef( header:read("*all"))
header:close()

local c_routines = C.load("Board")

EMPTY = c_routines.EMPTY
NUMBER = c_routines.NUMBER
WALL = c_routines.WALL
FILLED = c_routines.FILLED

HARD = 1
EASY = 0

function tile2int(tile)
   return c_routines.tile2int(tile)
end

function int2tile(int, tile)
   c_routines.int2tile(int, tile)
end

function Board.serialize(self)
   local ret = {
      deserialize = true,
      length = self.width*self.height,
      width = self.width,
      height = self.height,
      max_grid = {},
      min_tile_mask = {}
   }
   local max_grid = ret.max_grid
   local min_tile_mask = ret.min_tile_mask
   for i = 0,ret.length-1 do
      max_grid[i] = tile2int(self.max_grid.tiles[i])
      min_tile_mask[i] = self.min_tile_mask[i]
   end
   return ret
end

function Board.deserialize(table)
   assert(table)
   assert(table.deserialize)
   local ret = Board(table.width, table.height)

   local max_grid = table.max_grid
   local min_tile_mask = table.min_tile_mask
   for i = 0,ret.length-1 do
      ret.min_tile_mask[i] = min_tile_mask[i]
      if ret.min_tile_mask[i] == 1 then
         int2tile(max_grid[i], ret.min_grid.tiles[i])
      else
         int2tile(EMPTY, ret.min_grid.tiles[i])
      end
      int2tile(max_grid[i], ret.max_grid.tiles[i])
   end
   c_routines.Board_init_problem(ret, HARD)
   assert(ret.height ~= 0)
   return ret
end

function Board.create(ctype, width, height)
   local ret = c_routines.Board_create(width, height)
   return C.gc(ret, c_routines.Board_destroy)
end
function Board.print(self)
   return c_routines.Board_print(self)
end
function Board.maxify(self, max_tile)
   return c_routines.Board_maxify(self, max_tile)
end
function Board.init_problem(self, difficulty)
   return c_routines.Board_init_problem(self, difficulty)
end
function Board.destroy(self)
   c_routines.Board_destroy(self)
end
function Board.get_x(self, index)
   return c_routines.Board_get_x(self, index)
end
function Board.get_y(self, index)
   return c_routines.Board_get_y(self, index)
end
function Board.get_tile(self, index)
   return c_routines.Board_get_tile(self, index)
end
function Board.click(self, x, y, button)
   return c_routines.Board_click(self, x, y, button)
end
function Board.reduce(self, batch_size)
   return c_routines.Board_reduce(self, batch_size)
end
function Board.get_mistake(self)
   return c_routines.Board_get_mistake(self)
end
function Board.get_hint(self)
   local hint = c_routines.Board_get_hint(self)
   if hint.id < 0 then
      return nil
   else
      return hint
   end
end
function Board.is_solved(self)
   return 1 == c_routines.Board_is_solved(self)
end
function Board.undo(self)
   c_routines.Board_pop_change(self)
end
function Board.save(self, n_seconds)
   c_routines.Board_write(self, n_seconds)
end
function Board.load()
   n_seconds = C.new("unsigned[1]")
   board = c_routines.Board_read(n_seconds)
   if board ~= nil then
      return board, n_seconds[0]
   else
      return nil, 0
   end
end

local __board_mt = {
   __new = Board.create,
   __length = function(self) return self.length end,
   __index = Board
}

Board = C.metatype("struct Board", __board_mt)
