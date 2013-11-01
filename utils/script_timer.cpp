#include "global_lua.h"

#include "script_timer.h"

extern struct timer_mgr_s* global_timer_mgr;

template<>
ScriptTimerMgr* Singleton<ScriptTimerMgr>::g_pInstance = NULL;

ScriptTimer::ScriptTimer(const char* callback)
{
    m_callback = callback;
    m_timerID = -1;
}

void ScriptTimer::saveTimerID()
{
    m_timerID = getTimerID();
}

void ScriptTimer::onTimeOver()
{
    ScriptTimerMgr::GetInstance()->eraseTimer(m_timerID);

    lua_tinker::call<void>(global_lua_get(), m_callback.c_str(), m_timerID);
}

int script_timer_add(int delay, const char* calback)
{
    ScriptTimer* ptimer = new ScriptTimer(calback);
    int id = ptimer->start(global_timer_mgr, delay);
    ptimer->saveTimerID();

    ScriptTimerMgr::GetInstance()->pushTimer(id, ptimer);

    return id;
}

ScriptTimerMgr::ScriptTimerMgr()
{

}

void ScriptTimerMgr::pushTimer(int id, Timer* timer)
{
    m_timers[id] = timer;
}

void ScriptTimerMgr::eraseTimer(int id)
{
    m_timers.erase(id);
}

ScriptTimerMgr::~ScriptTimerMgr()
{
    for(unordered_map<int, Timer*>::iterator it = m_timers.begin(); it != m_timers.end(); ++it)
    {
        delete it->second;
    }

    m_timers.clear();
}