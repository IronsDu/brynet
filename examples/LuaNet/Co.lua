coObject = 
{
    next_co = nil, --活动队列中的下一个协程对象
    co = nil,      --协程对象
    status,  --当前的状态
    block = nil,   --阻塞结构
    timeout,
    index = 0,
    sc,      --归属的调度器
    data
}

function coObject:new(o)
  o = o or {}   
  setmetatable(o, self)
  self.__index = self
  self.status = "NONE"
  return o
end

function coObject:init(data,sc,co)
    self.data = data
    self.sc = sc
    self.co = co
end

function coObject:Signal(ev)
    if self.block ~= nil then
        if self.block:WakeUp(ev) then
            self.sc:Add2Active(self)
        end
    end
end


blockStruct = {
    bs_type,
}

function blockStruct:new(o)
  o = o or {}   
  setmetatable(o, self)
  self.__index = self
  return o
end

function blockStruct:WakeUp(type)
    return true
end