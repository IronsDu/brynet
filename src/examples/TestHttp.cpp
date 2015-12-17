#include <iostream>
#include <mutex>
#include <string>
#include <map>

#include "SocketLibFunction.h"
#include "HttpServer.h"
#include "SHA1.h"
#include "httprequest.h"
#include "base64.h"


#define WS_OPCODE_TEXT (0x1)
#define WS_OPCODE_BINARY (0x2)

bool ws_frame_extract(const string& frame, string& payload, byte& opcode)
{
    bool FIN = !!((byte)frame[0] & 0x80);
    opcode = (byte)frame[0] & 0x0F;
    bool MASK = !!((byte)frame[1] & 0x80);
    byte payloadlen1 = (byte)frame[1] & 0x7F;

    // we only want to handle frame:
    // 1.no fragment 2.masked 3.text or binary
    if (!FIN || !MASK || opcode != WS_OPCODE_TEXT && opcode != WS_OPCODE_BINARY)
        return false;

    u_int pos = 2;
    if (payloadlen1 == 126)
        pos = 4;
    else if (payloadlen1 == 127)
        pos = 10;

    byte mask[4];
    mask[0] = (byte)frame[pos++];
    mask[1] = (byte)frame[pos++];
    mask[2] = (byte)frame[pos++];
    mask[3] = (byte)frame[pos++];

    payload.resize(frame.size() - pos, 0);
    for (size_t i = pos, j = 0; i < frame.size(); i++, j++)
        payload[j] = frame[i] ^ mask[j % 4];

    return true;
}


bool ws_frame_build(const string& payload, string& frame)
{
    u_int payloadLen = payload.size();
    frame.clear();
    frame.push_back((char)0x81); // FIN = 1, opcode = BINARY
    if (payloadLen <= 125)
        frame.push_back((byte)payloadLen); // mask << 7 | payloadLen, mask = 0
    else if (payloadLen <= 0xFFFF)
    {
        frame.push_back(126);                       // 126 + 16bit len
        frame.push_back((payloadLen & 0xFF00) >> 8);
        frame.push_back(payloadLen & 0x00FF);
    }
    else
    {                                               // 127 + 64bit len
        frame.push_back(127);                       // assume payload len is less than u_int32_max
        frame.push_back(0x00);
        frame.push_back(0x00);
        frame.push_back(0x00);
        frame.push_back(0x00);
        frame.push_back((payloadLen & 0xFF000000) >> 24);
        frame.push_back((payloadLen & 0x00FF0000) >> 16);
        frame.push_back((payloadLen & 0x0000FF00) >> 8);
        frame.push_back(payloadLen & 0x000000FF);
    }

    frame.insert(frame.end(), payload.begin(), payload.end());
    return true;
}


bool ws_frame_assemble(byte* buf, u_int len, string& frame, u_int& readLen)
{
    u_int needLen = 2;
    if (len < needLen)
        return false;

    bool MASK = !!(buf[1] & 0x80);
    byte payloadlen1 = buf[1] & 0x7F;

    if (payloadlen1 == 126)
        needLen += 2;
    else if (payloadlen1 == 127)
        needLen += 8;

    if (MASK)
        needLen += 4;

    if (len < needLen)
        return false;

    u_int payloadLen = 0;
    if (payloadlen1 <= 125)
        payloadLen = payloadlen1;
    else if (payloadlen1 == 126)
        payloadLen = buf[2] << 8 |
        buf[3];
    else
        payloadLen = //buf[2] << 56 |
        //buf[3] << 48 |
        //buf[4] << 40 |
        //buf[5] << 32 |  // assume payload len is less than u_int32_max 
        buf[6] << 24 |
        buf[7] << 16 |
        buf[8] << 8 |
        buf[9];

    needLen += payloadLen;
    if (len < needLen)
        return false;

    frame = string((char*)buf, needLen);
    readLen = needLen;
    return true;
}


int main(int argc, char **argv)
{
    HttpServer server;

    server.start(8088, 1);

    server.setRequestHandle([](const HTTPProtocol& httpProtocol, TCPSession::PTR session, const char* extdata, int len){
        if (extdata != nullptr)
        {
            std::string frame(extdata, len);
            std::string payload;
            BYTE opcode;
            ws_frame_extract(frame, payload, opcode);
            opcode += 1;

            std::string sendPayload = "hahahello";
            std::string sendFrame;
            ws_frame_build(sendPayload, sendFrame);

            session->send(sendFrame.c_str(), sendFrame.size());
        }
        else
        {
            std::string secKey = httpProtocol.getValue("Sec-WebSocket-Key");
            secKey.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

            CSHA1 s1;
            s1.Update((unsigned char*)secKey.c_str(), secKey.size());
            s1.Final();
            unsigned char puDest[20];
            s1.GetHash(puDest);

            string base64Str = base64_encode((const unsigned char *)puDest, 20);

            string RESPONSE_TPL = "HTTP/1.1 101 Switching Protocols\r\n"
                "Upgrade: Websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Accept: ";

            RESPONSE_TPL += base64Str;
            RESPONSE_TPL += "\r\n\r\n";

            session->send(RESPONSE_TPL.c_str(), RESPONSE_TPL.size());
        }
    });

    sock fd = ox_socket_connect("180.87.33.107", 80);
    server.addConnection(fd, [](TCPSession::PTR session){

        /*∑¢ÀÕhttp request*/

    }, [](const HTTPProtocol& httpProtocol, TCPSession::PTR session, const char* extdata, int len){
        /*¥¶¿Ìresponse*/
    });

    std::cin.get();
}