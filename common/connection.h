#ifndef _CONNECTION_H
#define _CONNECTION_H

#include <stdint.h>
#include "dllconf.h"

#ifdef  __cplusplus
extern "C" {
#endif

/*  单客户端(只链接一个服务器)网络库 */

struct connection_s;

enum net_msg_type
{
    net_msg_establish,          /*  链接建立成功  */
    net_msg_connect_failed,     /*  链接失败        */
    net_msg_close,              /*  链接断开    */
    net_msg_data,               /*  链接接收数据  */
};

struct msg_data_s
{
    char type;             /*  消息包类型,库使用者禁止修改此字段       */
    int msg_id;
    int len;                            /*  消息包长度,库使用者序列化数据时修改此字段   */
    char data[1];                       /*  消息包内容,库使用者序列化数据到此缓冲区    */
};

/*  网络库缓冲池配置参数  */
struct connection_msgpool_config
{
    const int*    sendmsg_pool_num;     /*  各种类的消息包默认个数  */
    const int*    sendmsg_pool_len;     /*  各种类的长度  */
    char    sendmsg_pool_typemax;       /*  长度配置个数  */

    const int*    netmsg_pool_num;      /*  各种类的消息包默认个数 */
    const int*    netmsg_pool_len;      /*  各种类的长度  */
    char    netmsg_pool_typemax;        /*  长度配置个数  */
};

/*  检查数据是否包含完整包 */
typedef int (*pfn_check_packet)(const char* buffer, int len);
/*  包处理函数类型 */
typedef void (*pfn_packet_handle)(struct connection_s* self, struct msg_data_s*, void* ext);

/*  创建客户端网络    */
DLL_CONF    struct connection_s* ox_connection_new(int rdsize, int sdsize, pfn_check_packet, pfn_packet_handle, const struct connection_msgpool_config config, void* ext);

/*  释放客户端网络实例:非线程安全 */
DLL_CONF    void ox_connection_delete(struct connection_s* self);

/*  申请消息包   */
DLL_CONF    struct msg_data_s* ox_connection_sendmsg_claim(struct connection_s* self, int pool_index);

/*  根据长度申请合适的消息包   */
DLL_CONF    struct msg_data_s* ox_connection_sendmsg_lenclaim(struct connection_s* self, int data_len);

/*  返回消息类型最大容量  */
DLL_CONF    int ox_connection_config_datalen(struct connection_s* self, struct msg_data_s* msg);

/*  发送消息包   */
DLL_CONF    void ox_connection_send(struct connection_s* self, struct msg_data_s* msg);

/*  请求链接服务器 */
DLL_CONF    void ox_connection_send_connect(struct connection_s* self, const char* ip, int port, int timeout);
/*  请求断开与服务器的链接 */
DLL_CONF    void ox_connection_send_close(struct connection_s* self);

/*  网络层调度   */
DLL_CONF    void ox_connection_netpoll(struct connection_s* self, int64_t millisecond);
/*  逻辑层调度处理网络层分发的网络数据(如果当前没有消息,则函数返回,但保证此函数至少执行timeout毫秒)   */
DLL_CONF    void ox_connection_logicpoll(struct connection_s* self, int64_t millisecond);

#ifdef  __cplusplus
}
#endif

#endif
