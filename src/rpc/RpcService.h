#ifndef DODO_RPC_RPCSERVICE_H_
#define DODO_RPC_RPCSERVICE_H_

#include <string>
#include <functional>

namespace dodo
{
    namespace rpc
    {
        using namespace std;

        template <typename T>
        class HasCallOperator
        {
            typedef char _One;
            typedef struct{ char a[2]; }_Two;
            template<typename TT>
            static _One hasFunc(decltype(&TT::operator()));
            template<typename TT>
            static _Two hasFunc(...);
        public:
            static const bool value = sizeof(hasFunc<T>(nullptr)) == sizeof(_One);
        };

        const static string RPC_REPLY_STR = "rpc_reply";

        template<typename PROTOCOL_TYPE>
        class RpcService
        {
        public:
            RpcService()
            {
                /*  注册rpc_reply 服务函数，处理rpc返回值   */
                def(RPC_REPLY_STR.c_str(), [this](int req_id, const string& response){
                    handleResponse(response);
                    mResponseCallbacks.del(std::to_string(req_id));
                });
            }

            template<typename F>
            void        def(const char* funname, F func)
            {
                mRpcFunctions.insertFunction(funname, func);
            }

            /*  远程调用，返回值为经过序列化后的消息  */
            template<typename... Args>
            string    call(const char* funname, const Args&... args)
            {
                return mCaller.call(mResponseCallbacks, funname, args...);
            }

            /*  处理rpc请求 */
            void    handleRpc(const string& str)
            {
                mRpcFunctions.execute(str.c_str(), str.size());
            }

            void    handleRpc(const char* str, size_t len)
            {
                mRpcFunctions.execute(str, len);
            }

            /*  返回数据给RPC调用端    */
            template<typename... Args>
            string    reply(int reqid, const Args&... args)
            {
                /*  把实际返回值打包作为参数,调用对端的rpc_reply 函数*/
                return call(RPC_REPLY_STR.c_str(), reqid, call(std::to_string(reqid).c_str(), args...));
            }

        private:
            /*  调用方处理收到的rpc返回值(消息)*/
            void    handleResponse(const string& str)
            {
                mResponseCallbacks.execute(str.c_str(), str.size());
            }

        private:
            typename PROTOCOL_TYPE::FunctionMgr      mResponseCallbacks;
            typename PROTOCOL_TYPE::FunctionMgr      mRpcFunctions;
            typename PROTOCOL_TYPE::Caller           mCaller;
        };
    }
}

#endif