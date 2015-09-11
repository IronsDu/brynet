#ifndef _DATASOCKET_H
#define _DATASOCKET_H

#include <memory>
#include <functional>
#include <deque>

#include "SocketLibFunction.h"
#include "Channel.h"
#include "timer.h"
#include "EventLoop.h"

#ifdef USE_OPENSSL

#ifdef  __cplusplus
extern "C" {
#endif
#include "openssl/ssl.h"
#ifdef  __cplusplus
}
#endif

#endif

class EventLoop;
struct buffer_s;

class DataSocket : public Channel
{
public:
    typedef DataSocket*                                                         PTR;

    typedef std::function<void(PTR)>                                            ENTER_CALLBACK;
    typedef std::function<int(PTR, const char* buffer, int len)>                DATA_CALLBACK;
    typedef std::function<void(PTR)>                                            DISCONNECT_CALLBACK;

    typedef std::shared_ptr<std::string>                                        PACKET_PTR;

public:
    explicit DataSocket(int fd);
    ~DataSocket();

    bool                            onEnterEventLoop(EventLoop* el);

    void                            send(const char* buffer, int len);

    void                            sendPacket(const PACKET_PTR&);
    void                            sendPacket(PACKET_PTR&&);

    void                            setEnterCallback(ENTER_CALLBACK cb);
    void                            setDataCallback(DATA_CALLBACK cb);
    void                            setDisConnectCallback(DISCONNECT_CALLBACK cb);

    void                            setCheckTime(int overtime);
    /*主动(投递)断开连接,会触发断开回调*/
    void                            postDisConnect();

    void                            setUserData(int64_t value);
    int64_t                         getUserData() const;

#ifdef USE_OPENSSL
    void                            setupAcceptSSL(SSL_CTX*);
    void                            setupConnectSSL();
#endif

    static  PACKET_PTR              makePacket(const char* buffer, int len);
private:

    void                            PingCheck();
    void                            startPingCheckTimer();

    void                            canRecv() override;
    void                            canSend() override;

    bool                            checkRead();
    bool                            checkWrite();

    void                            recv();
    void                            flush();
    void                            normalFlush();
    void                            quickFlush();

    void                            onClose() override;
    void                            closeSocket();
    void                            procCloseInLoop();

    void                            runAfterFlush();
#ifdef PLATFORM_LINUX
    void                            removeCheckWrite();
#endif

private:

#ifdef PLATFORM_WINDOWS
    #include <windows.h>
    struct EventLoop::ovl_ext_s     mOvlRecv;
    struct EventLoop::ovl_ext_s     mOvlSend;
    struct EventLoop::ovl_ext_s     mOvlClose;

    bool                            mPostRecvCheck;     /*  是否投递了可读检测   */
    bool                            mPostWriteCheck;    /*  是否投递了可写检测   */
#endif

    int                             mFD;
    bool                            mIsPostFinalClose;  /*  是否投递了最终的close处理    */

    bool                            mCanWrite;          /*  socket是否可写  */

    EventLoop*                      mEventLoop;
    buffer_s*                       mRecvBuffer;

    struct pending_packet
    {
        PACKET_PTR  packet;
        size_t      left;
    };

    typedef std::deque<pending_packet>   PACKET_LIST_TYPE;
    PACKET_LIST_TYPE                mSendList;          /*  发送消息列表  */

    ENTER_CALLBACK                  mEnterCallback;
    DATA_CALLBACK                   mDataCallback;
    DISCONNECT_CALLBACK             mDisConnectCallback;

    bool                            mIsPostFlush;       /*  是否已经放置flush消息的回调    */

    int64_t                         mUserData;          /*  链接的用户自定义数据  */

#ifdef USE_OPENSSL
    SSL_CTX*                        mSSLCtx;
    SSL*                            mSSL;
#endif

    bool                            mRecvData;
    int                             mCheckTime;
    Timer::WeakPtr                  mTimer;
};

#endif