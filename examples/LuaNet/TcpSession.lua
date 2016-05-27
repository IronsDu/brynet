require "Scheduler"

local TcpSession = {
}

local function TcpSessionNew(p)
	local o = {}
	setmetatable(o, p)
	p.__index = p
	
	o.serviceID = -1
	o.socketID = -1
	o.recvPackets = {}
	o.isClosed = false
	o.recvCo = nil
	o.unpackCallback = nil
	o.server = nil

	return o
end

function TcpSession:init(serviceID, socketID)
	self.serviceID = serviceID
	self.socketID = socketID
end

function TcpSession:setServer(server)
	self.server = server
end

function TcpSession:getServer()
	return self.server
end

function TcpSession:setUnpack(unpackcallabck)
	self.unpackCallback = unpackcallabck
end

function TcpSession:setClose()
	self.isClosed = true
end

function TcpSession:isClose()
	return self.isClosed
end

function TcpSession:parseData(data, len)
	if self.unpackCallback == nil then
		return 0
	end

	local totalLen = 0
	while totalLen < len do
		local packLen = self.unpackCallback(data, totalLen, len)
		if packLen > 0 then
			table.insert(self.recvPackets, string.sub(data, totalLen, packLen))
			totalLen = totalLen + packLen
		else
			break
		end
	end
			
	if totalLen > 0 then
		self:wakeupRecv()
	end

	return totalLen
end

function TcpSession:recv(timeout)
	if timeout == nil then
		timeout = 1000
	end

	self.recvCo = coroutine_running()

	local hasData = false
	local packet = nil

	if next(self.recvPackets) == nil then
		self.recvCo.waitType = "WAIT_RECV"
		coroutine_sleep(self.recvCo, timeout)
		hasData = next(self.recvPackets) ~= nil
	else
		hasData = true
	end
	self.recvCo = nil
	
	if hasData then
		packet = self.recvPackets[1]
		table.remove(self.recvPackets, 1)
	end

	return packet
end

function TcpSession:wakeupRecv()
	if self.recvCo ~= nil then
		coroutine_wakeup(self.recvCo, "WAIT_RECV")
	end
end

function TcpSession:send(data)
	CoreDD:sendToTcpSession(self.serviceID, self.socketID, data, string.len(data))
end

return {
	New = function () return TcpSessionNew(TcpSession) end
}