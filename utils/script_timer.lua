local _timer_table = {}

function scripttimer_enter_function(timer_id)
    local temp_table = _timer_table[timer_id]
    local callback = temp_table[1]
    _timer_table[timer_id] = nil
    if callback ~= nil then
        callback(temp_table[2])
    end
end

function scripttimer_add(delay, callback, ...)
    local timer_id = cpptimer_new(delay, "scripttimer_enter_function")
    if timer_id > 0 then
        _timer_table[timer_id] = {callback, arg}
    end
end