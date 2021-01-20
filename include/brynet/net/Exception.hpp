#pragma once

#include <stdexcept>
#include <string>

namespace brynet { namespace net {

class ConnectException : public std::runtime_error
{
public:
    explicit ConnectException(const std::string& message)
        : std::runtime_error(message)
    {
    }

    explicit ConnectException(const char* message)
        : std::runtime_error(message)
    {
    }
};

class BrynetCommonException : public std::runtime_error
{
public:
    explicit BrynetCommonException(const std::string& message)
        : std::runtime_error(message)
    {
    }

    explicit BrynetCommonException(const char* message)
        : std::runtime_error(message)
    {
    }
};

}}// namespace brynet::net
