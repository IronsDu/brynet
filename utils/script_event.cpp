#include "script_event.h"

HasScriptEvent::HasScriptEvent()
{
    static int  _temp = 0;
    m_key = _temp++;
}

HasScriptEvent::~HasScriptEvent()
{
    delAllListener();
}

int HasScriptEvent::getKey() const
{
    return m_key;
}

void HasScriptEvent::delAllListener()
{
    if(m_key != -1)
    {
        lua_tinker::call<void>(global_lua_get(), "eventobject_del", m_key);
    }

    m_key = -1;
}