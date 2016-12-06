#ifndef DODO_NET_CHANNEL_H_
#define DODO_NET_CHANNEL_H_

namespace dodo
{
    namespace net
    {
        class EventLoop;

        class Channel
        {
        public:
            virtual ~Channel(){}

        private:
            virtual void    canSend() = 0;
            virtual void    canRecv() = 0;
            virtual void    onClose() = 0;

            friend class EventLoop;
        };
    }
}

#endif