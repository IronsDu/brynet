#pragma once

#include <string>
#include <exception>
#include <stdexcept>

namespace brynet { namespace net {

    class ConnectException : public std::runtime_error
    {
    public:
        explicit ConnectException(const std::string& message)
            :
            std::runtime_error(message)
        {
        }

        explicit ConnectException(const char* message)
            :
            std::runtime_error(message)
        {
        }
    };

    class BrynetCommonException : public std::runtime_error
    {
    public:
        explicit BrynetCommonException(const std::string& message)
            :
            std::runtime_error(message)
        {
        }

        explicit BrynetCommonException(const char* message)
            :
            std::runtime_error(message)
        {
        }
    };

} }