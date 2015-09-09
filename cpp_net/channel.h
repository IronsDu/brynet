#ifndef _CHANNEL_H
#define _CHANNEL_H

class EventLoop;

/*TODO::是否将 canSend，canRecv以及onClose整合成一个函数，可以在函数里立即处理*/
class Channel
{
public:
    virtual ~Channel(){}
    virtual void    canSend() = 0;
    virtual void    canRecv() = 0;
    virtual void    onClose() = 0;
};

#endif