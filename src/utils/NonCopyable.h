#ifndef __NON_COPYABLE_H__
#define __NON_COPYABLE_H__

class NonCopyable
{
protected:
    NonCopyable()   {}
    ~NonCopyable()  {}

private:
    NonCopyable(const NonCopyable&) = delete;
    const NonCopyable& operator=(const NonCopyable&) = delete;
};

#endif