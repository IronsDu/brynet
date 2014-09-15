#ifndef _WEBCLIENT_H
#define _WEBCLIENT_H

#include <stack>
#include <list>
#include <string>
using namespace std;

#include "connector.h"

class HttpHelp
{
public:
    string static getipofhost(const char* host)
    {
        string ret;

        char ip[IP_SIZE];
        struct hostent *hptr = gethostbyname(host);
        if(hptr != NULL)
        {
            if(hptr->h_addrtype == AF_INET || hptr->h_addrtype == AF_INET6)
            {
                char* lll = *(hptr->h_addr_list);
                sprintf(ip, "%d.%d.%d.%d", lll[0]&0x00ff, lll[1]&0x00ff, lll[2]&0x00ff, lll[3]&0x00ff);
                ret = ip;
            }
        }

        return ret;
    }

    string static gethttp_chunked_data(const char* buffer)
    {
        string ret;

        const char* response_end = strstr(buffer, "\r\n\r\n");
        if(response_end != NULL)
        {
            response_end += strlen("\r\n\r\n");

            const char* fuck_start = response_end;
            const char* len_flag = strstr(fuck_start, "\r\n");

            while(len_flag != NULL)
            {
                string tmp(fuck_start, len_flag);
                int    nValude    =   0;         
                sscanf(tmp.c_str(),"%x",&nValude); 

                if(nValude == 0)
                {
                    break;
                }

                len_flag += strlen("\r\n");
                fuck_start = len_flag;

                const char* data_line = fuck_start;

                fuck_start = strstr(len_flag, "\r\n");

                if(fuck_start != NULL)
                {
                    ret += string(data_line, fuck_start);

                    fuck_start += strlen("\r\n");
                    len_flag = strstr(fuck_start, "\r\n");
                }
                else
                {
                    break;
                }
            }
        }

        return ret;
    }

    static  int check_packet(const char* buffer, int len)
    {
        int ret_len = 0;

        if (len >= 4 && buffer[0] == 'H' && buffer[1] == 'T' && buffer[2] == 'T' && buffer[3] == 'P')    /*  应该是http协议了吧?    */
        {
            const char* find = strstr(buffer, "Content-Length");
            if (find != NULL)
            {
                const char* findenter = strstr(find, "\r\n");
                if (findenter)
                {
                    char temp[1024];
                    memset(temp, 0, sizeof(temp));

                    const char* datastr_start = find + strlen("Content-Length: ");
                    int datastr_len = findenter - datastr_start;

                    for (int i = 0; i < datastr_len; ++i)
                    {
                        temp[i] = datastr_start[i];
                    }

                    int datalen = atoi(temp);

                    const char* response_end = strstr(find, "\r\n\r\n");
                    if (response_end != NULL)
                    {
                        response_end += strlen("\r\n\r\n");

                        int response_head_len = response_end - buffer;
                        int left_len = len - response_head_len;

                        if (left_len == datalen)
                        {
                            ret_len = len;
                        }
                        else
                        {
                            ret_len = 0;
                        }
                    }
                }
            }
            else
            {
                const char* has_chunked = strstr(buffer, "Transfer-Encoding: chunked");
                if (has_chunked)
                {
                    const char* response_end = strstr(buffer, "\r\n\r\n");
                    if (response_end != NULL)
                    {
                        response_end += strlen("\r\n\r\n");

                        const char* fuck_start = response_end;
                        const char* len_flag = strstr(fuck_start, "\r\n");

                        while (len_flag != NULL)
                        {
                            string tmp(fuck_start, len_flag);
                            int    nValude = 0;
                            sscanf(tmp.c_str(), "%x", &nValude);

                            if (nValude == 0)
                            {
                                ret_len = len;
                                break;
                            }

                            len_flag += strlen("\r\n");
                            fuck_start = len_flag;

                            fuck_start = strstr(len_flag, "\r\n");
                            if (fuck_start != NULL)
                            {
                                fuck_start += strlen("\r\n");
                                len_flag = strstr(fuck_start, "\r\n");
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                }
            }
        }

        return ret_len;
    }
};

class CppServer;

/* 发起异步http 链接    */
class HttpConnector : public Connector
{
public:
    HttpConnector(CppServer* server);

    /*  pop一个http请求附加数据 */
    ConnectorBaseReq*           popRequestUD();

private:
    virtual void                onConnectFailed(ConnectorBaseReq* ud);
    virtual void                onConnectSuccess(sock fd, ConnectorBaseReq* ud);

private:
    CppServer*                  m_server;

    /*  投递的（未完成链接）的请求队列 */
    list<ConnectorBaseReq*>     m_requestDatas;

    bool                        m_canConnectHttpServer; /*  能否链接到web服务器 */
};

#endif