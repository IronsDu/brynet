#ifndef DODO_RPC_MEMBERRPC_H_
#define DODO_RPC_MEMBERRPC_H_

#include <string>

#include "JsonRpc.h"
#include "MsgpackRpc.h"
#include "RpcService.h"

namespace dodo
{
    namespace rpc
    {
        template<typename T, typename RPCPROTOCOL = dodo::rpc::MsgpackProtocol >
        class MemberRpcService : public dodo::rpc::RpcService < RPCPROTOCOL>
        {
        public:
            MemberRpcService()
            {
                mObj = nullptr;
            }

            void        setObject(T* p)
            {
                mObj = p;
            }

            template<typename... Args>
            void        registerHandle(std::string name, void (T::*callback)(Args...))
            {
                def(name.c_str(), [this, callback](Args... args){
                    if (mObj != nullptr)
                    {
                        (mObj->*callback)(args...);
                    }
                    mObj = nullptr;
                });
            }

            template<typename PBType, typename... Args>
            void        registerPBHandle(void (T::*callback)(PBType, Args...))
            {
                registerHandle(std::remove_reference<PBType>::type::descriptor()->full_name(), callback);
            }

        private:
            T*         mObj;
        };
    }
}

#endif