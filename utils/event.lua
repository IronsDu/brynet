--事件系统主表,key 为对象id，value为其所有事件处理器
local eventsystem_table = {}

--给对象添加事件处理器
function eventobject_addlistener(eventobj, event_id, func)
    local object_id = eventobj:toHasScriptEvent():getKey()
    local listeners = eventsystem_table[object_id]
    if listeners == nil then
        eventsystem_table[object_id] = {}
        listeners = eventsystem_table[object_id]
    end

    local subtable = listeners[event_id]
    if subtable == nil then
        listeners[event_id] = {}
        subtable = listeners[event_id]
    end

    table.insert(subtable, func)
end

--对象触发事件
function eventobject_fireevent(object_id, event_id, ...)
    local listeners = eventsystem_table[object_id]
    if listeners ~= nil then
        local subtable = listeners[event_id]
        if subtable ~= nil then
            for k,v in pairs(subtable) do
                v(...)
            end
        end
    end
end

--清除此事件对象
function eventobject_del(object_id)
    if eventsystem_table[object_id] ~= nil then
        eventsystem_table[object_id] = nil
    end
end

--删除事件对象的一个处理器
function eventobject_removelistener(eventobj, event_id, func)
    local object_id = eventobj:toHasScriptEvent():getKey()
    local listeners = eventsystem_table[object_id]
    if listeners ~= nil then
        local subtable = listeners[event_id]
        if subtable ~= nil then
            for k,v in pairs(subtable) do
                if v == func then
                    subtable[k] = nil
                    return
                end
            end
        end
    end
end
