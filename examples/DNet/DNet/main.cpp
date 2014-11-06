#include <functional>
#include <time.h>
#include <stdio.h>
#include <thread>
#include <iostream>

#include "eventloop.h"
#include "datasocket.h"

int
ox_socket_connect(const char* server_ip, int port)
{
    struct sockaddr_in server_addr;
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);

    if (clientfd != SOCKET_ERROR)
    {
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(server_ip);
        server_addr.sin_port = htons(port);

        while (connect(clientfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) < 0)
        {
            if (EINTR == GetLastError())
            {
                continue;
            }
        }
    }

    return clientfd;
}

int
ox_socket_listen(int port, int back_num)
{
    int socketfd = SOCKET_ERROR;
    struct  sockaddr_in server_addr;
    int reuseaddr_value = 1;

    socketfd = socket(AF_INET, SOCK_STREAM, 0);

    if (socketfd != SOCKET_ERROR)
    {
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = INADDR_ANY;

        setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuseaddr_value, sizeof(int));

        if (::bind(socketfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == SOCKET_ERROR ||
            listen(socketfd, back_num) == SOCKET_ERROR)
        {
            closesocket(socketfd);
            socketfd = SOCKET_ERROR;
        }
    }

    return socketfd;
}


int main()
{
    WSADATA g_WSAData;
    WSAStartup(MAKEWORD(2, 2), &g_WSAData);

    /*  服务器端eventloop   */
    EventLoop serverEventLoop;

    int total_recv_len = 0;

    /*  监听线程    */
    std::thread listenthread([&]{
        while (true)
        {
            int client_fd = SOCKET_ERROR;
            struct sockaddr_in socketaddress;
            socklen_t size = sizeof(struct sockaddr);

            int listen_fd = ox_socket_listen(8888, 25);

            if (SOCKET_ERROR != listen_fd)
            {
                for (; ;)
                {
                    while ((client_fd = accept(listen_fd, (struct sockaddr*)&socketaddress, &size)) < 0)
                    {
                        if (EINTR == GetLastError())
                        {
                            continue;
                        }
                    }

                    if (SOCKET_ERROR != client_fd)
                    {
                        printf("accept new client:%d\n", client_fd);
                        int flag = 1;
                        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
                        {
                            int sd_size = 32 * 1024;
                            int op_size = sizeof(sd_size);
                            setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, (const char*)&sd_size, op_size);
                        }

                        Channel* channel = new DataSocket(client_fd);
                        serverEventLoop.addConnection(client_fd, channel, [&](Channel* arg){
                            DataSocket* ds = static_cast<DataSocket*>(arg);
                            ds->setEventLoop(&serverEventLoop);
                            printf("in net loop, on enter client \n");

                            /*  可以放入消息队列，然后唤醒它主线程的eventloop，然后主线程通过消息队列去获取*/

                            ds->setDataHandle([&total_recv_len](DataSocket* ds, const char* buffer, int len){

                                /*  reply client    */
                                total_recv_len += len;
                                ds->send(buffer, len);
                            });
                        });
                    }
                }

                closesocket(listen_fd);
                listen_fd = SOCKET_ERROR;
            }
            else
            {
                printf("listen failed\n");
            }
        }
    });

    /*  客户端IO线程   */
    std::thread clientthread([]{
        /*  客户端eventloop*/
        EventLoop clientEventLoop;

        /*  消息包大小定义 */
        #define PACKET_LEN (1024 * 10)
        #define CLIENT_NUM (5000)

        const char* senddata = (const char*)malloc(PACKET_LEN);

        for (int i = 0; i < CLIENT_NUM; i++)
        {
            int client = ox_socket_connect("127.0.0.1", 8888);
            clientEventLoop.addConnection(client, new DataSocket(client), [&](Channel* arg){
                DataSocket* ds = static_cast<DataSocket*>(arg);
                ds->setEventLoop(&clientEventLoop);

                /*  发送消息    */
                ds->send(senddata, PACKET_LEN);

                /*  可以放入消息队列，然后唤醒它主线程的eventloop，然后主线程通过消息队列去获取*/
                ds->setDataHandle([](DataSocket* ds, const char* buffer, int len){
                    ds->send(buffer, len);
                });
            });
        }

        while (true)
        {
            clientEventLoop.loop(-1);
        }
    });

    /*  主线程处理服务器端IO*/
    DWORD lasttime = GetTickCount();
    while (true)
    {
        serverEventLoop.loop(-1);

        DWORD now = GetTickCount();
        if ((now - lasttime) >= 1000)
        {
            cout << "recv :" << (total_recv_len / 1024) << " K/s" << endl;
            lasttime = now;
            total_recv_len = 0;
        }
    }
}