#ifndef _CHANNEL_H
#define _CHANNEL_H

class Channel
{
public:
    virtual void    canSend() = 0;
    virtual void    canRecv() = 0;
};

#endif