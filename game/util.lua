--- Memoizer ------------------------
local Memo = {}
Memo.__call = function( self, ...)
   local hash = self.h( ...)
   if  self.tab[hash] then
      return self.tab[hash]
   else
      self.tab[hash] = self.f( ...)
      return self.tab[hash]
   end
end

Memo.__index = Memo

Memo.new = function( fun, hash)
   return setmetatable( { f = fun, h = hash, tab = {}}, Memo)
end

Memo.clear = function(self)
   self.tab = {}
end

-------------------------------------
--- Fake Cache ----------------------
--- wipes itself when it gets too big
local FakeCache = {}
FakeCache.__index = FakeCache
FakeCache.__call = function( self, ...)
   local hash = self.h( ...)
   if  self.tab[hash] then
      return self.tab[hash]
   else
      self.n = self.n + 1
      if self.n == self.size then
         self:clear()
         self.n = 1
      end
      self.tab[hash] = self.f( ...)
      return self.tab[hash]
   end
end

FakeCache.new = function( arg)
   return setmetatable( { f = arg.method,
                          h = arg.key,
                          tab = {},
                          size = arg.size, -- if nil the FakeCache never clears
                          n = 0,
                        }, FakeCache)
end

FakeCache.clear = function(self)
   self.n = 0
   self.tab = {}
end

return {Memo = Memo, Cache = FakeCache}
