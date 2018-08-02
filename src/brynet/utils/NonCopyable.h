#ifndef BRYNET_NONCOPYABLE_H_
#define BRYNET_NONCOPYABLE_H_

namespace brynet
{
    class NonCopyable
    {
    protected:
        NonCopyable() = default;
        ~NonCopyable() = default;

        NonCopyable(const NonCopyable&) = delete;
        const NonCopyable& operator=(const NonCopyable&) = delete;
    };
}

#endif