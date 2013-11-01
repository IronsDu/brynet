#ifndef _SCRIPT_TIMER_H
#define _SCRIPT_TIMER_H

#include <string>
#include <unordered_map>
using namespace std;

#include "singleton.h"
#include "timer.h"

class ScriptTimer : public Timer
{
public:
    ScriptTimer(const char* callback);
    void            saveTimerID();

private:
    void            onTimeOver();

private:
    string          m_callback;
    int             m_timerID;
};

class ScriptTimerMgr : public Singleton<ScriptTimerMgr>
{
public:
    ScriptTimerMgr();
    ~ScriptTimerMgr();

    void            pushTimer(int id, Timer* timer);
    void            eraseTimer(int id);

private:
    unordered_map<int, Timer*>  m_timers;
};

int     script_timer_add(int delay, const char* calback);

#endif