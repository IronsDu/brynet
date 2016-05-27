require "scheduler"
local AsyncConnect = require "Connect"
local TcpSession = require "TcpSession"

local __TcpServiceList = {}

function __on_enter__(serviceID, socketID)
	__TcpServiceList[serviceID].entercallback(serviceID, socketID)
end

function __on_close__(serviceID, socketID)
	__TcpServiceList[serviceID].closecallback(serviceID, socketID)
end

function __on_data__(serviceID, socketID, data, len)
	return __TcpServiceList[serviceID].datacallback(serviceID, socketID, data, len)
end

function __on_connected__(serviceID, socketID, uid)
	__TcpServiceList[serviceID].connected(serviceID, socketID, uid)
end

local TcpService = {
}

local function TcpServiceNew(p)
	local o = {}
	setmetatable(o, p)
	p.__index = p
	
	o.serviceID = -1
	o.acceptSessions = {}
	o.sessions = {}
	o.connectedCo = {}
	o.connectedSessions = {}
	o.connectedUnpacks = {}
	
	o.entercallback = nil
	o.closecallback = nil
	o.datacallback = nil
	o.connected = nil

	return o
end

function TcpService:findSession(socketID)
	return self.sessions[socketID]
end

function TcpService:createService()
	if self.serviceID ~= -1 then
		return
	end

	local serviceID = CoreDD:createTCPService()
	self.serviceID = serviceID
	__TcpServiceList[serviceID] = self

	local server = self

	self.closecallback = function (serviceID, socketID)
		local session = server.sessions[socketID]
		if session ~= nil then
			session:setClose(true)
			session:wakeupRecv()
		end

		server.sessions[socketID] = nil
	end

	self.datacallback = function (serviceID, socketID, data, len)
		local session = server.sessions[socketID]
		return session:parseData(data, len)
	end

	self.connected = function (serviceID, socketID, uid)
		local waitCo = self.connectedCo[uid]
		if waitCo ~= nil then
			local session = TcpSession:New()
			session:init(serviceID, socketID)
			session:setServer(server)
			session:setUnpack(server.connectedUnpacks[uid])
			server.connectedUnpacks[uid] = nil

			server.connectedSessions[uid] = session
			server.sessions[socketID] = session
			self.connectedCo[uid] = nil
			coroutine_wakeup(waitCo, "WAIT_ESTABLISH")
		else
			CoreDD:closeTcpSession(serviceID, socketID)
		end
	end
end

function TcpService:listen(ip, port, unpackcallback)
	self:createService()
	if self.entercallback  == nil then
		CoreDD:listen(self.serviceID, ip, port)	--开启监听服务
		local server = self

		self.entercallback = function (serviceID, socketID)
			local session = TcpSession:New()
			session:init(serviceID, socketID)
			session:setServer(server)
			session:setUnpack(unpackcallback)

			table.insert(server.acceptSessions, session)
			server.sessions[socketID] = session
			server:wakeupAccept()
		end
	end
end

function TcpService:connect(ip, port, timeout, unpackcallback)
	local server = self

	local uid = AsyncConnect.AsyncConnect(ip, port, timeout, function (fd, uid)
		if fd == -1 then
			local waitCo = server.connectedCo[uid]
			if waitCo ~= nil then
				self.connectedCo[uid] = nil
				coroutine_wakeup(waitCo, "WAIT_ESTABLISH")
			end
		else
			CoreDD:addSessionToService(server.serviceID, fd, uid)
		end
	end)

	server.connectedUnpacks[uid] = unpackcallback
	server.connectedCo[uid] = coroutine_running()
	local co = coroutine_running()
	co.waitType = "WAIT_ESTABLISH"
	coroutine_sleep(co, timeout)
	--寻找uid对应的session
	local session = self.connectedSessions[uid]
	self.connectedSessions[uid] = nil
	self.connectedCo[uid] = nil
	self.connectedUnpacks[uid] = nil

	return session
end

function TcpService:startCoService(enterHandler, msgHandler, closeHandler)
	--TODO::将参数传入协程启动函数
	local server = self
	coroutine_start(function()
		while true do
			local session = server:accept()
			if session ~= nil then
				coroutine_start(function ()
					if enterHandler ~= nil then
						enterHandler(session)
					end

					while true do
						local packet = session:recv()
						if packet ~= nil then
							msgHandler(session, packet)
						end

						if session:isClose() then
							if closeHandler ~= nil then
								closeHandler(session)
							end
							break
						end
					end
				end)
			end
		end
	end)
end

function TcpService:accept(timeout)
	if timeout == nil then
		timeout = 1000
	end

	local newClient = nil
	local hasData = false

	self.acceptCo = coroutine_running()
	if next(self.acceptSessions) == nil then
		self.acceptCo.waitType = "WAIT_ACCEPT"
		coroutine_sleep(self.acceptCo, timeout)
		hasData = next(self.acceptSessions) ~= nil
	else
		hasData = true
	end

	self.acceptCo = nil
	
	if hasData then
		newClient = self.acceptSessions[1]
		table.remove(self.acceptSessions, 1)
	end

	return newClient
end

function TcpService:wakeupAccept()
	if self.acceptCo ~= nil then
		coroutine_wakeup(self.acceptCo, "WAIT_ACCEPT")
	end
end

return {
	New = function () return TcpServiceNew(TcpService) end
}