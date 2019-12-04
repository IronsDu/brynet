#pragma once

namespace brynet { namespace net {

    class EventLoop;

    class Channel
    {
    public:
        virtual ~Channel() = default;

    private:
        virtual void    canSend() = 0;
        virtual void    canRecv() = 0;
        virtual void    onClose() = 0;

        friend class EventLoop;
    };

} }