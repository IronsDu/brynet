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
        ERROR_FRAME = 0xff,
        CONTINUATION_FRAME = 0x00,
        TEXT_FRAME = 0x01,
        BINARY_FRAME = 0x02,
        CLOSE_FRAME = 0x08,
        PING_FRAME = 0x09,
        PONG_FRAME = 0x0A
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

    static bool wsFrameBuild(const std::string& payload, std::string& frame, WebSocketFrameType frame_type = WebSocketFrameType::TEXT_FRAME, bool isFin = true, bool masking = false)
    {
        static_assert(std::is_same<std::string::value_type, char>::value, "");

        uint8_t head = (uint8_t)frame_type | (isFin ? 0x80: 0x00);

        size_t payloadLen = payload.size();
        frame.clear();
        frame.push_back((char)head);
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

        if (masking)
        {
            frame[1] = ((uint8_t)frame[1]) | 0x80;
            uint8_t mask[4];
            for (size_t i = 0; i < sizeof(mask) / sizeof(mask[0]); i++)
            {
                mask[i] = rand();
                frame.push_back(mask[i]);
            }

            std::string maskPayload;
            for (size_t i = 0; i < payload.size(); i++)
            {
                maskPayload.push_back((uint8_t)payload[i] ^ mask[i % 4]);
            }
            
            frame.insert(frame.end(), maskPayload.begin(), maskPayload.end());
        }
        else
        {
            frame.insert(frame.end(), payload.begin(), payload.end());
        }

        return true;
    }

    static bool wsFrameExtractBuffer(const char* inbuffer, const size_t bufferSize, std::string& payload, WebSocketFrameType& outopcode, size_t& frameSize, bool& outfin)
    {
        const unsigned char* buffer = (const unsigned char*)inbuffer;

        if (bufferSize < 2)
        {
            return false;
        }

        const bool isFinish = (buffer[0] & 0x80) != 0;
        const WebSocketFrameType frameType = (WebSocketFrameType)(buffer[0] & 0x0F);
        const bool isMasking = (buffer[1] & 0x80) != 0;
        uint32_t payloadlen = buffer[1] & 0x7F;

        uint32_t pos = 2;
        if (payloadlen == 126)
        {
            if (bufferSize < 4)
            {
                return false;
            }

            payloadlen = (buffer[2] << 8) + buffer[3];
            pos = 4;
        }
        else if (payloadlen == 127)
        {
            if (bufferSize < 10)
            {
                return false;
            }

            payloadlen = (buffer[6] << 24) + (buffer[7] << 16) + (buffer[8] << 8) + buffer[9];
            pos = 10;
        }

        uint8_t mask[4];
        if (isMasking)
        {
            if (bufferSize < (pos + 4))
            {
                return false;
            }

            mask[0] = buffer[pos++];
            mask[1] = buffer[pos++];
            mask[2] = buffer[pos++];
            mask[3] = buffer[pos++];
        }

        if (bufferSize < (pos + payloadlen))
        {
            return false;
        }
        
        if (isMasking)
        {
            payload.resize(payloadlen, 0);
            for (size_t i = pos, j = 0; j < payloadlen; i++, j++)
                payload[j] = buffer[i] ^ mask[j % 4];
        }
        else
        {
            payload.append((const char*)buffer, pos, payloadlen);
        }

        frameSize = payloadlen + pos;
        outopcode = frameType;
        outfin = isFinish;

        return true;
    }

    static bool wsFrameExtractString(const std::string& buffer, std::string& payload, WebSocketFrameType& opcode, size_t& frameSize, bool& isFin)
    {
        return wsFrameExtractBuffer(buffer.c_str(), buffer.size(), payload, opcode, frameSize, isFin);
    }
};

#endif