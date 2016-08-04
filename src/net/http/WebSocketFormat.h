#ifndef _WEBSOCKET_FORMAT_H
#define _WEBSOCKET_FORMAT_H

#include <string>
#include <stdint.h>

#include "SHA1.h"
#include "base64.h"


#define WS_OPCODE_TEXT (0x1)
#define WS_OPCODE_BINARY (0x2)

class WebSocketFormat
{
public:
    static std::string wsHandshake(std::string secKey)
    {
        secKey.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

        CSHA1 s1;
        s1.Update((unsigned char*)secKey.c_str(), secKey.size());
        s1.Final();
        unsigned char puDest[20];
        s1.GetHash(puDest);

        std::string base64Str = base64_encode((const unsigned char *)puDest, 20);

        std::string response = "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: Websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: ";

        response += base64Str;
        response += "\r\n\r\n";

        return response;
    }

    static bool wsFrameBuild(const std::string& payload, std::string& frame)
    {
        uint32_t payloadLen = payload.size();
        frame.clear();
        frame.push_back((char)0x81); // FIN = 1, opcode = BINARY
        if (payloadLen <= 125)
            frame.push_back((uint8_t)payloadLen); // mask << 7 | payloadLen, mask = 0
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

    static bool wsFrameExtractBuffer(const char* buffer, size_t bufferSize, std::string& payload, uint8_t& opcode, int& frameSize)
    {
        if (bufferSize < 2)
        {
            return false;
        }

        bool FIN = ((uint8_t)buffer[0] & 0x80) != 0;
        opcode = (uint8_t)buffer[0] & 0x0F;
        bool MASK = ((uint8_t)buffer[1] & 0x80) != 0;
        uint8_t payloadlen1 = (uint8_t)buffer[1] & 0x7F;

        // we only want to handle frame:
        // 1.no fragment 2.masked 3.text or binary
        if (!FIN && !MASK && opcode != WS_OPCODE_TEXT && opcode != WS_OPCODE_BINARY)
            return false;

        uint32_t pos = 2;
        if (payloadlen1 == 126)
            pos = 4;
        else if (payloadlen1 == 127)
            pos = 10;
        uint8_t mask[4];
        if (MASK)
        {
            if (bufferSize < (pos + 4))
            {
                return false;
            }

            mask[0] = (uint8_t)buffer[pos++];
            mask[1] = (uint8_t)buffer[pos++];
            mask[2] = (uint8_t)buffer[pos++];
            mask[3] = (uint8_t)buffer[pos++];
        }

        if (bufferSize < (pos + payloadlen1))
        {
            return false;
        }
        
        if (MASK)
        {
            payload.resize(payloadlen1, 0);
            for (size_t i = pos, j = 0; j < payloadlen1; i++, j++)
                payload[j] = buffer[i] ^ mask[j % 4];
        }
        else
        {
            payload.append(buffer, pos, payloadlen1);
        }

        frameSize = payloadlen1 + pos;

        return true;
    }

    static bool wsFrameExtractString(const std::string& buffer, std::string& payload, uint8_t& opcode, int& frameSize)
    {
        return wsFrameExtractBuffer(buffer.c_str(), buffer.size(), payload, opcode, frameSize);
    }
};

#endif