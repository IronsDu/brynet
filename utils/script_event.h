#ifndef _SCRIPT_EVENT_H
#define _SCRIPT_EVENT_H

#include <stdint.h>
#include "global_lua.h"

enum SCRIPT_EVENT_TYPE
{
    EVENT_TYPE_ATTACKED,    /*  ±»¹¥»÷ÊÂ¼þ   */
};

#define SCRIPT_EVENT_ENTERFUNCTION_NAME "eventobject_fireevent"
class HasScriptEvent
{
public:
    HasScriptEvent();
    virtual ~HasScriptEvent();

    int             getKey() const;

    void            delAllListener();

    template<typename RVal>
    RVal fireEvent(SCRIPT_EVENT_TYPE event_type)
    {
        return lua_tinker::call<RVal>(global_lua_get(), SCRIPT_EVENT_ENTERFUNCTION_NAME, m_key, event_type);
    }

    template<typename RVal, typename T1>
    RVal fireEvent(SCRIPT_EVENT_TYPE event_type, T1 arg1)
    {
        return lua_tinker::call<RVal>(global_lua_get(), SCRIPT_EVENT_ENTERFUNCTION_NAME, m_key, event_type, arg1);
    }

    template<typename RVal, typename T1, typename T2>
    RVal fireEvent(SCRIPT_EVENT_TYPE event_type, T1 arg1, T2 arg2)
    {
        return lua_tinker::call<RVal>(global_lua_get(), SCRIPT_EVENT_ENTERFUNCTION_NAME, m_key, event_type, arg1, arg2);
    }

    template<typename RVal, typename T1, typename T2, typename T3>
    RVal fireEvent(SCRIPT_EVENT_TYPE event_type, T1 arg1, T2 arg2, T3 arg3)
    {
        return lua_tinker::call<RVal>(global_lua_get(), SCRIPT_EVENT_ENTERFUNCTION_NAME, m_key, event_type, arg1, arg2, arg3);
    }

    template<typename RVal, typename T1, typename T2, typename T3, typename T4>
    RVal fireEvent(SCRIPT_EVENT_TYPE event_type, T1 arg1, T2 arg2, T3 arg3, T4 arg4)
    {
        return lua_tinker::call<RVal>(global_lua_get(), SCRIPT_EVENT_ENTERFUNCTION_NAME, m_key, event_type, arg1, arg2, arg3, arg4);
    }

    template<typename RVal, typename T1, typename T2, typename T3, typename T4, typename T5>
    RVal fireEvent(SCRIPT_EVENT_TYPE event_type, T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5)
    {
        return lua_tinker::call<RVal>(global_lua_get(), SCRIPT_EVENT_ENTERFUNCTION_NAME, m_key, event_type, arg1, arg2, arg3, arg4, arg5);
    }

private:
    int             m_key;
};

#define _DEFINE_HASSCRIPTEVENT_MEMBERFUNCTION HasScriptEvent*   toHasScriptEvent()                      \
                                                                {                                       \
                                                                    return (HasScriptEvent*)this;       \
                                                                }

#endif