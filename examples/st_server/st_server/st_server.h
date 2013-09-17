#ifndef _ST_SERVER_H
#define _ST_SERVER_H

#include "dllconf.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

    /*  单线程服务器网络库   */

    struct st_server_s;

    /*  网络库缓冲池配置参数  */
    struct st_server_msgpool_config
    {
        const int*    msg_pool_num;     /*  各种类的消息包默认个数  */
        const int*    msg_pool_len;     /*  各种类的长度  */
        char    msg_pool_typemax;       /*  长度配置个数  */
    };

    /*  消息包类型   */
    struct st_server_packet_s
    {
        int ref;                            /*  消息包引用计数,库使用者禁止修改此字段     */
        int data_len;                       /*  消息包长度,库使用者序列化数据时修改此字段   */
        char data[1];                       /*  消息包内容,库使用者序列化数据到此缓冲区    */
    };

    /*  检查数据是否包含完整包 */
    typedef int (*pfn_check_packet)(const char* buffer, int len);
    /*  包处理函数类型 */
    typedef void (*pfn_packet_handle)(struct st_server_s* self, int index, const char* buffer, int len);

    /*  链接进入时回调函数类型 */
    typedef void (*pfn_session_onenter)(struct st_server_s* self, int index);
    /*  链接断开时回调函数类型 */
    typedef void (*pfn_session_onclose)(struct st_server_s* self, int index);

    DLL_CONF struct st_server_s* ox_stserver_create(
        int num,                            /*  服务器最大回话容量 */
        int rdsize,                         /*  会话内置接收缓冲区大小 */
        int sdsize,                         /*  会话内置发送缓冲区大小 */
        pfn_check_packet packet_check,      /*  包完整性检查函数        */
        pfn_packet_handle packet_handle,    /*  包处理回调函数         */
        pfn_session_onenter onenter,        /*  链接进入时处理函数   */
        pfn_session_onclose onclose,        /*  链接断开时处理函数   */
        const struct st_server_msgpool_config   /*  缓冲池配置   */
        );

    /*  网络调度    */
    DLL_CONF void ox_stserver_poll(struct st_server_s* self, int timeout);
    /*  网络释放    */
    DLL_CONF void ox_stserver_delete(struct st_server_s* self);

    /*  断开某会话链接 */
    DLL_CONF void ox_stserver_close(struct st_server_s*, int index);

    /*  申请消息包   */
    DLL_CONF struct st_server_packet_s* ox_stserver_claim_packet(struct st_server_s* self, int pool_index);
    /*  根据数据长度申请适合大小的消息包   */
    DLL_CONF struct st_server_packet_s* ox_stserver_claim_lenpacket(struct st_server_s* self, int len);
    /*  发送消息包   */
    DLL_CONF bool ox_stserver_send(struct st_server_s* self, struct st_server_packet_s* packet, int index);
    /*  刷新消息包   */
    DLL_CONF void ox_stserver_flush(struct st_server_s* self, int index);
    /*  释放消息包   */
    DLL_CONF void ox_stserver_check_reclaim(struct st_server_s* self, struct st_server_packet_s* packet);

    /*  添加socket    */
    DLL_CONF void ox_stserver_addfd(struct st_server_s* self, int fd);

#ifdef __cplusplus
}
#endif

#endif