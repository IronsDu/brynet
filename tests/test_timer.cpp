#define CATCH_CONFIG_MAIN// This tells Catch to provide a main() - only do this in one cpp file
#include <brynet/base/Timer.hpp>
#include <brynet/base/WaitGroup.hpp>
#include <chrono>
#include <memory>
#include <thread>

#include "catch.hpp"

TEST_CASE("Timer are computed", "[timer]")
{
    auto timerMgr = std::make_shared<brynet::base::TimerMgr>();

    int upvalue = 0;
    auto timer = timerMgr->addTimer(std::chrono::seconds(1), [&upvalue]() {
        upvalue++;
    });
    std::function<void(void)> fuck;
    bool testCancelFlag = false;
    timer = timerMgr->addTimer(std::chrono::seconds(1), [&upvalue, &fuck]() {
        fuck();
        upvalue++;
    });

    fuck = [&]() {
        auto t = timer.lock();
        if (t != nullptr)
        {
            t->cancel();
            testCancelFlag = true;
        }
    };
    auto leftTime = timerMgr->nearLeftTime();
    REQUIRE_FALSE(leftTime > std::chrono::seconds(1));
    REQUIRE(timerMgr->isEmpty() == false);
    while (!timerMgr->isEmpty())
    {
        timerMgr->schedule();
    }
    REQUIRE(upvalue == 2);
    REQUIRE(testCancelFlag);

    timer = timerMgr->addTimer(std::chrono::seconds(1), [&upvalue]() {
        upvalue++;
    });
    timer.lock()->cancel();
    REQUIRE(timerMgr->isEmpty() == false);
    while (!timerMgr->isEmpty())
    {
        timerMgr->schedule();
    }

    REQUIRE(upvalue == 2);
}

TEST_CASE("repeat timer are computed", "[repeat timer]")
{
    auto timerMgr = std::make_shared<brynet::base::TimerMgr>();
    auto wg = brynet::base::WaitGroup::Create();
    wg->add(1);

    std::atomic_int value = ATOMIC_VAR_INIT(0);
    auto timer = timerMgr->addIntervalTimer(std::chrono::milliseconds(100), [&]() {
        if (value.load() < 10)
        {
            value.fetch_add(1);
        }
        else
        {
            wg->done();
        }
    });

    std::thread t([&]() {
        wg->wait();
        timer->cancel();
    });
    while (!timerMgr->isEmpty())
    {
        timerMgr->schedule();
    }
    t.join();
    REQUIRE(value.load() == 10);
}
