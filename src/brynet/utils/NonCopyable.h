#pragma once

namespace brynet { namespace utils {

    class NonCopyable
    {
    public:
        NonCopyable(const NonCopyable&) = delete;
        const NonCopyable& operator=(const NonCopyable&) = delete;

    protected:
        NonCopyable() = default;
        ~NonCopyable() = default;
    };

} }