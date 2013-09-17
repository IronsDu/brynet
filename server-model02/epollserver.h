#ifndef _EPOLLSERVER_H_INCLUDED
#define _EPOLLSERVER_H_INCLUDED

#ifdef  __cplusplus
extern "C" {
#endif

    struct server_s;

    //  session_recvbuffer_size :   内置接收缓冲区大小
    //  session_sendbuffer_size :   内置发送缓冲区大小(保存直接投递未成功的数据)   

    struct server_s* epollserver_create(
        int max_num,
        int session_recvbuffer_size,
        int session_sendbuffer_size,
        void*   ext
        );

    void epollserver_delete(struct server_s* self);

#ifdef  __cplusplus
}
#endif

#endif
