#ifndef _BRYNET_NET_PROMISE_RECEIVE_H
#define _BRYNET_NET_PROMISE_RECEIVE_H

#include <brynet/net/WrapTCPService.h>

namespace brynet
{
    namespace net
    {
        /* binary search in memory */
        int memsearch(const char *hay, int haysize, const char *needle, int needlesize)
        {
            int haypos, needlepos;
            haysize -= needlesize;

            for (haypos = 0; haypos <= haysize; haypos++)
            {
                for (needlepos = 0; needlepos < needlesize; needlepos++)
                {
                    if (hay[haypos + needlepos] != needle[needlepos])
                    {
                        // Next character in haystack.
                        break;
                    }
                }
                if (needlepos == needlesize)
                {
                    return haypos;
                }
            }

            return -1;
        }

        class PromiseRecieve;

        std::shared_ptr<PromiseRecieve> setupPromiseReceive(const TCPSession::PTR& session);

        class PromiseRecieve : public std::enable_shared_from_this<PromiseRecieve>
        {
        public:
            typedef std::shared_ptr<PromiseRecieve> PTR;
            typedef std::function<bool(const char* buffer, size_t len)> Handle;

            PromiseRecieve::PTR receive(size_t len, Handle handle)
            {
                return receive(std::make_shared<size_t>(len), std::move(handle));
            }

            PromiseRecieve::PTR receive(std::shared_ptr<size_t> len, Handle handle)
            {
                if (*len < 0)
                {
                    throw std::runtime_error("len less than zero");
                }

                return helpReceive(std::move(len), "", std::move(handle));
            }

            PromiseRecieve::PTR receiveUntil(std::string str, Handle handle)
            {
                if (str.empty())
                {
                    throw std::runtime_error("str is empty");
                }

                return helpReceive(nullptr, std::move(str), std::move(handle));
            }

        private:
            PromiseRecieve::PTR helpReceive(std::shared_ptr<size_t> len, std::string str, Handle handle)
            {
                auto pr = std::make_shared<PendingReceive>();
                pr->len = std::move(len);
                pr->str = std::move(str);
                pr->handle = std::move(handle);
                mPendingReceives.push_back(std::move(pr));

                return shared_from_this();
            }

            size_t process(const char* buffer, size_t len)
            {
                size_t procLen = 0;

                while (!mPendingReceives.empty() && len >= procLen)
                {
                    auto pendingReceive = mPendingReceives.front();
                    if (pendingReceive->len != nullptr)
                    {
                        auto tryReceiveLen = *pendingReceive->len;
                        if (tryReceiveLen < (len - procLen))
                        {
                            break;
                        }

                        mPendingReceives.pop_front();
                        procLen += tryReceiveLen;
                        if (pendingReceive->handle(buffer + procLen - tryReceiveLen, tryReceiveLen) && tryReceiveLen > 0)
                        {
                            mPendingReceives.push_front(pendingReceive);
                        }
                    }
                    else if (!pendingReceive->str.empty())
                    {
                        auto pos = memsearch(buffer + procLen,
                            len-procLen, 
                            pendingReceive->str.c_str(), 
                            pendingReceive->str.size());

                        if (pos < 0)
                        {
                            break;
                        }

                        mPendingReceives.pop_front();
                        auto findLen = pos + pendingReceive->str.size();
                        procLen += findLen;
                        if (pendingReceive->handle(buffer + procLen - findLen, findLen))
                        {
                            mPendingReceives.push_front(pendingReceive);
                        }
                    }
                    else
                    {
                        break;
                    }
                }

                return procLen;
            }

        private:
            struct PendingReceive
            {
                std::shared_ptr<size_t> len;
                std::string str;
                Handle handle;
            };

            std::deque<std::shared_ptr<PendingReceive>> mPendingReceives;

            friend std::shared_ptr<PromiseRecieve> setupPromiseReceive(const TCPSession::PTR& session);
        };

        std::shared_ptr<PromiseRecieve> setupPromiseReceive(const TCPSession::PTR& session)
        {
            auto promiseReceive = std::make_shared<PromiseRecieve>();
            session->setDataCallback([promiseReceive](const TCPSession::PTR& session, 
                const char* buffer, 
                size_t len) {
                return promiseReceive->process(buffer, len);
            });

            return promiseReceive;
        }
    }
}

#endif
