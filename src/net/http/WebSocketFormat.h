#ifndef _WEBSOCKET_FORMAT_H
#define _WEBSOCKET_FORMAT_H

#include <string>
#include <stdint.h>

#include "SHA1.h"
#include "base64.h"

class WebSocketFormat
{
public:
    enum class WebSocketFrameType {
        ERROR_FRAME = 0xFF00,
        INCOMPLETE_FRAME = 0xFE00,

        OPENING_FRAME = 0x3300,
        CLOSING_FRAME = 0x3400,

        INCOMPLETE_TEXT_FRAME = 0x01,
        INCOMPLETE_BINARY_FRAME = 0x02,

        TEXT_FRAME = 0x81,
        BINARY_FRAME = 0x82,

        PING_FRAME = 0x19,
        PONG_FRAME = 0x1A
    };

    static std::string wsHandshake(std::string secKey)
    {
        secKey.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

        CSHA1 s1;
        s1.Update((unsigned char*)secKey.c_str(), static_cast<unsigned int>(secKey.size()));
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

    static bool wsFrameBuild(const std::string& payload, std::string& frame, WebSocketFrameType frame_type = WebSocketFrameType::TEXT_FRAME)
    {
        size_t payloadLen = payload.size();
        frame.clear();
        frame.push_back((char)frame_type);
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
            frame.push_back(static_cast<char>((payloadLen & 0xFF000000) >> 24));
            frame.push_back(static_cast<char>((payloadLen & 0x00FF0000) >> 16));
            frame.push_back(static_cast<char>((payloadLen & 0x0000FF00) >> 8));
            frame.push_back(static_cast<char>(payloadLen & 0x000000FF));
        }

        frame.insert(frame.end(), payload.begin(), payload.end());
        return true;
    }

    static bool wsFrameExtractBuffer(const char* buffer, size_t bufferSize, std::string& payload, WebSocketFrameType& outopcode, int& frameSize)
    {
        if (bufferSize < 2)
        {
            return false;
        }

        bool FIN = ((uint8_t)buffer[0] & 0x80) != 0;
        uint8_t opcode = (uint8_t)buffer[0] & 0x0F;
        bool MASK = ((uint8_t)buffer[1] & 0x80) != 0;
        uint8_t payloadlen = (uint8_t)buffer[1] & 0x7F;

        uint32_t pos = 2;
        if (payloadlen == 126)
            pos = 4;
        else if (payloadlen == 127)
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

        if (bufferSize < (pos + payloadlen))
        {
            return false;
        }
        
        if (MASK)
        {
            payload.resize(payloadlen, 0);
            for (size_t i = pos, j = 0; j < payloadlen; i++, j++)
                payload[j] = buffer[i] ^ mask[j % 4];
        }
        else
        {
            payload.append(buffer, pos, payloadlen);
        }

        frameSize = payloadlen + pos;

        if (opcode == 0x0)
        {
            outopcode = (FIN) ? WebSocketFrameType::TEXT_FRAME : WebSocketFrameType::INCOMPLETE_TEXT_FRAME;
        }
        else if (opcode == 0x01)
        {
            outopcode = (FIN) ? WebSocketFrameType::TEXT_FRAME : WebSocketFrameType::INCOMPLETE_TEXT_FRAME;
        }
        else if (opcode == 0x02)
        {
            outopcode = (FIN) ? WebSocketFrameType::BINARY_FRAME : WebSocketFrameType::INCOMPLETE_BINARY_FRAME;
        }
        else if (opcode == 0x09)
        {
            outopcode = WebSocketFrameType::PING_FRAME;
        }
        else if (opcode == 0x0A)
        {
            outopcode = WebSocketFrameType::PONG_FRAME;
        }

        return true;
    }

    static bool wsFrameExtractString(const std::string& buffer, std::string& payload, WebSocketFrameType& opcode, int& frameSize)
    {
        return wsFrameExtractBuffer(buffer.c_str(), buffer.size(), payload, opcode, frameSize);
    }
};

#endif