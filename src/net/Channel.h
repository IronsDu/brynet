#ifndef _CHANNEL_H
#define _CHANNEL_H

class EventLoop;

class Channel
{
public:
    virtual ~Channel(){}
    virtual void    canSend() = 0;
    virtual void    canRecv() = 0;
    virtual void    onClose() = 0;
};

#endif