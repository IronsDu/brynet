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
		--开启http服务器
		local serverService = TcpService:New()
		serverService:listen("0.0.0.0", 80)
		coroutine_start(function()
			while true do
				local session = serverService:accept()
				if session ~= nil then
					coroutine_start(function ()

						--读取报文头
						local packet = session:receiveUntil("\r\n")

						--读取多行头部
						while true do
							packet = session:receiveUntil("\r\n")
							if packet ~= nil then
								print(packet)
								if #packet == 0 then
									print("recv empty line")
									break
								end
							end
						end

						local htmlBody = "<html><head><title>This is title</title></head><body>hahaha</body></html>"
						local response = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/html\r\nContent-Length: "..string.len(htmlBody).."\r\n\r\n"..htmlBody

						session:send(response)
					end)
				end
			end
		end)
	end

	if true then
		--开启服务器
		local serverService = TcpService:New()
		serverService:listen("0.0.0.0", 81)
		coroutine_start(function()
			while true do
				local session = serverService:accept()
				if session ~= nil then
					coroutine_start(function ()
						_myenter(session)

						while true do
							local packet = session:receive(5)	--读取5个字节
							if packet ~= nil then
								_mymsg(session, packet)
							end

							if session:isClose() then
								_myclose(session)
								break
							end
						end
					end)
				end
			end
		end)
	end

	if true then
		--开启10个客户端
		local clientService = TcpService:New()
		clientService:createService()
		
		for i=1,1 do
			coroutine_start(function ()
				local session = clientService:connect("127.0.0.1", 81, 5000)

				if session ~= nil then
					session:send("hello")
					while true do
					local packet = session:receive(5)
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