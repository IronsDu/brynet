#include <memory>
#include <string>

namespace brynet { namespace net {

class SendableMsg
{
public:
    using Ptr = std::shared_ptr<SendableMsg>;

    virtual ~SendableMsg() = default;

    virtual const void* data() = 0;
    virtual size_t size() = 0;
};

class StringSendMsg : public SendableMsg
{
public:
    explicit StringSendMsg(const char* buffer, size_t len)
        : mMsg(buffer, len)
    {}

    explicit StringSendMsg(const std::string& buffer)
        : mMsg(buffer)
    {}

    explicit StringSendMsg(std::string&& buffer)
        : mMsg(std::move(buffer))
    {}

    const void* data() override
    {
        return static_cast<const void*>(mMsg.data());
    }
    size_t size() override
    {
        return mMsg.size();
    }

private:
    std::string mMsg;
};

static SendableMsg::Ptr MakeStringMsg(const char* buffer, size_t len)
{
    return std::make_shared<StringSendMsg>(buffer, len);
}

static SendableMsg::Ptr MakeStringMsg(const std::string& buffer)
{
    return std::make_shared<StringSendMsg>(buffer);
}

static SendableMsg::Ptr MakeStringMsg(std::string&& buffer)
{
    return std::make_shared<StringSendMsg>(std::move(buffer));
}

}}// namespace brynet::net