local TcpService = require "TcpService"
local AcyncConnect = require "Connect"

function _myenter(session)
	print("enter")
end

function _myclose(session)
	print("close")
end

function _mymsg(session, packet)
	if false then
		print("recv packet:"..packet)
		local htmlBody = "<html><head><title>This is title</title></head><body>hahaha</body></html>"
		local response = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/html\r\nContent-Length: "..string.len(htmlBody).."\r\n\r\n"..htmlBody

		session:send(response)
	end

	session:send(packet)

	--[[
	Redis.get
	]]
end

local totalRecvNum = 0

function userMain()
	if true then
		--开启服务器
		local serverService = TcpService:New()
		serverService:listen("0.0.0.0", 80, function (data, startPos, len)
				return len --总是认为收到完整包
			end)
		serverService:startCoService(_myenter, function (session, packet) session:send(packet) end, _myclose)
	end

	if true then
		--开启10个客户端
		local clientService = TcpService:New()
		clientService:createService()
		
		for i=1,10 do
			coroutine_start(function ()
				local session = clientService:connect("127.0.0.1", 80, 5000, function (data, startPos, len)
					return len
				end)

				if session ~= nil then
					session:send("hello")
					while true do
					local packet = session:recv()
						if packet ~= nil then
							totalRecvNum = totalRecvNum + 1
							session:send(packet)
						end

						if session:isClose() then
							break
						end
					end
				else
					print("connect failed")
				end
			end)
		end
	end

	coroutine_start(function ()
			while true do
				coroutine_sleep(coroutine_running(), 1000)
				print("total recv :"..totalRecvNum.."/s")
				totalRecvNum = 0
			end
		end)
end

coroutine_start(function ()
	userMain()
end)

while true
do
	CoreDD:loop()
	while coroutine_pengdingnum() > 0
	do
		coroutine_schedule()
	end
end