#ifndef _IOCP_H_INCLUDED
#define _IOCP_H_INCLUDED

#ifdef  __cplusplus
extern "C" {
#endif

struct server_s;

//  session_recvbuffer_size : 内置接收缓冲区大小
//  session_sendbuffer_size : 内置发送缓冲区大小(存放投递未成功的数据)

struct server_s* iocp_create(
    int port, 
    int max_num,
    int session_recvbuffer_size,
    int session_sendbuffer_size
    );

void iocp_delete(struct server_s* self);

#ifdef  __cplusplus
}
#endif

#endif
