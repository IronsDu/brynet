#ifndef _TCP_SERVER_H
#define _TCP_SERVER_H

#include "eventloop.h"
#include "datasocket.h"

#include <vector>
#include <assert.h>

template<typename T>
class IdTypes
{
public:
    int         claimID()
    {
        int ret = -1;

        if (mIds.empty())
        {
            increase();
        }

        assert(!mIds.empty());

        if (!mIds.empty())
        {
            ret = mIds[mIds.size() - 1];
            mIds.pop_back();
        }
        
        return ret;
    }

    void        reclaimID(size_t id)
    {
        assert(id >= 0 && id < mValues.size());
        mIds.push_back(id);
    }

    void        set(T* t, size_t id)
    {
        assert(id >= 0 && id < mValues.size());
        if (id >= 0 && id < mValues.size())
        {
            mValues[id] = t;
        }
    }

    T*          get(size_t id)
    {
        T* ret = nullptr;
        assert(id >= 0 && id < mValues.size());
        if (id >= 0 && id < mValues.size())
        {
            ret = mValues[id];
        }

        return ret;
    }

private:
    void                increase()
    {
        const static int _NUM = 100;

        int oldsize = mValues.size();
        if (oldsize > 0)
        {
            mValues.resize(oldsize + _NUM, nullptr);
            for (int i = 0; i < _NUM; i++)
            {
                mIds.push_back(oldsize + i);
            }
        }
        else
        {
            mValues.resize(oldsize + _NUM, nullptr);
            for (int i = 0; i < _NUM; i++)
            {
                mIds.push_back(oldsize + i);
            }
        }
    }

private:
    std::vector<T*>     mValues;
    std::vector<size_t> mIds;
};

class TcpServer
{
    typedef std::function<void (EventLoop&)> FRAME_CALLBACK;
    typedef std::function<void(int64_t)>    CONNECTION_ENTER_HANDLE;
    typedef std::function<void(int64_t)>    DISCONNECT_PROC;
    typedef std::function<void(int64_t, const char* buffer, int len)>    DATA_PROC;

public:
    TcpServer(int port, int threadNum, FRAME_CALLBACK callback = nullptr);  /*callback为IO线程每个loop循环都会执行的回调函数，可以为null*/
    ~TcpServer();

    void                                setEnterHandle(TcpServer::CONNECTION_ENTER_HANDLE handle);
    void                                setDisconnectHandle(TcpServer::DISCONNECT_PROC handle);
    void                                setMsgHandle(TcpServer::DATA_PROC handle);

    void                                send(int64_t id, DataSocket::PACKET_PTR& packet);

    /*主动断开此id链接，但仍然可能收到此id的断开回调，需要上层逻辑自己处理这个"问题"*/
    void                                disConnect(int64_t id);

private:
    void                                RunListen(int port);
    int64_t                             MakeID(int loopIndex);

    void                                _procDataSocketClose(DataSocket*);

private:
    EventLoop*                          mLoops;
    std::thread**                       mIOThreads;
    int                                 mLoopNum;
    std::thread*                        mListenThread;

    IdTypes<DataSocket>*                mIds;
    int*                                mIncIds;

    TcpServer::CONNECTION_ENTER_HANDLE  mEnterHandle;
    TcpServer::DISCONNECT_PROC          mDisConnectHandle;
    TcpServer::DATA_PROC                mDataProc;

    union SessionId
    {
        struct
        {
            int16_t loopIndex;
            int16_t index;
            int32_t iid;
        }data;

        int64_t id;
    };
};

#endif