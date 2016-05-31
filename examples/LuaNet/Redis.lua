local TcpService = require "TcpService"
local AcyncConnect = require "Connect"

local commands = {
    "append",            "auth",              "bgrewriteaof",
    "bgsave",            "bitcount",          "bitop",
    "blpop",             "brpop",
    "brpoplpush",        "client",            "config",
    "dbsize",
    "debug",             "decr",              "decrby",
    "del",               "discard",           "dump",
    "echo",
    "eval",              "exec",              "exists",
    "expire",            "expireat",          "flushall",
    "flushdb",           "get",               "getbit",
    "getrange",          "getset",            "hdel",
    "hexists",           "hget",              "hgetall",
    "hincrby",           "hincrbyfloat",      "hkeys",
    "hlen",
    "hmget",             --[[ "hmset", ]]     "hscan",
    "hset",
    "hsetnx",            "hvals",             "incr",
    "incrby",            "incrbyfloat",       "info",
    "keys",
    "lastsave",          "lindex",            "linsert",
    "llen",              "lpop",              "lpush",
    "lpushx",            "lrange",            "lrem",
    "lset",              "ltrim",             "mget",
    "migrate",
    "monitor",           "move",              "mset",
    "msetnx",            "multi",             "object",
    "persist",           "pexpire",           "pexpireat",
    "ping",              "psetex",       --[[ "psubscribe", ]]
    "pttl",
    "publish",      --[[ "punsubscribe", ]]   "pubsub",
    "quit",
    "randomkey",         "rename",            "renamenx",
    "restore",
    "rpop",              "rpoplpush",         "rpush",
    "rpushx",            "sadd",              "save",
    "scan",              "scard",             "script",
    "sdiff",             "sdiffstore",
    "select",            "set",               "setbit",
    "setex",             "setnx",             "setrange",
    "shutdown",          "sinter",            "sinterstore",
    "sismember",         "slaveof",           "slowlog",
    "smembers",          "smove",             "sort",
    "spop",              "srandmember",       "srem",
    "sscan",
    "strlen",       --[[ "subscribe", ]]      "sunion",
    "sunionstore",       "sync",              "time",
    "ttl",
    "type",         --[[ "unsubscribe", ]]    "unwatch",
    "watch",             "zadd",              "zcard",
    "zcount",            "zincrby",           "zinterstore",
    "zrange",            "zrangebyscore",     "zrank",
    "zrem",              "zremrangebyrank",   "zremrangebyscore",
    "zrevrange",         "zrevrangebyscore",  "zrevrank",
    "zscan",
    "zscore",            "zunionstore",       "evalsha"
}

local RedisSession = {
}

for i=1,#commands do
	local cmd = commands[i]
	RedisSession[cmd] = function (self, ... )
		return self:_do_cmd(cmd, ...)
	end
end

local function RedisSessionNew(p)
	local o = {}
	setmetatable(o, p)
	p.__index = p
	
	o.tcpsession = nil

	return o
end

function RedisSession:connect(tcpservice, ip, port, timeout)
	self.tcpsession = tcpservice:connect(ip, port, timeout)
	return self.tcpsession ~= nil
end

function RedisSession:sendRequest(...)
	local request = ""

	local args = {...}
	local nargs = #args
	request = "*" .. nargs .. "\r\n"
	for i = 1, nargs do
		local arg = args[i]
		if type(arg) ~= "string" then
            arg = tostring(arg)
        end

		request = request .. "$" .. #arg .. "\r\n" .. arg .. "\r\n"
	end
	self.tcpsession:send(request)
end

function RedisSession:recvReply()

	local line = self.tcpsession:receiveUntil("\r\n")
	if line == nil then
		return false, "server close"
	end

	local prefix = string.byte(line, 1)
	if prefix == 36 then	-- $

		local size = tonumber(string.sub(line, 2))
		local data = self.tcpsession:receive(size)
		self.tcpsession:receive(2)
		return data

	elseif prefix == 43 then  -- +
		return string.sub(line, 2)
	elseif prefix == 42 then  -- *

		local vals = {}
		local n = tonumber(string.sub(line, 2))
		for i=1,n do
			local res, err = self:recvReply()
			if res  then
			elseif res == nil then
				return nil, err
			else
				vals[i] = res
			end
		end

		return vals

	elseif prefix == 58 then  -- :
		return tonumber(string.sub(line, 2))
	elseif prefix == 45 then  -- -
		return false, string.sub(line, 2)
	else
		print("prefix error")
		return nil, "unkown prefix: " .. tostring(prefix)
	end
end

function RedisSession:_do_cmd(...)
	if self.tcpsession == nil then
		return nil , "not connection"
	end

	self:sendRequest(...)
	return self:recvReply()
end

return {
	New = function () return RedisSessionNew(RedisSession) end
}