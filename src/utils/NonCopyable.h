#ifndef DODO_NONCOPYABLE_H_
#define DODO_NONCOPYABLE_H_

namespace dodo
{
    class NonCopyable
    {
    protected:
        NonCopyable()   {}
        ~NonCopyable()  {}

    private:
        NonCopyable(const NonCopyable&) = delete;
        const NonCopyable& operator=(const NonCopyable&) = delete;
    };
}

#endif