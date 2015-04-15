#ifndef _MSGPACKRPC_H
#define _MSGPACKRPC_H

#include <stdio.h>
#include <assert.h>
#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <functional>
#include <tuple>

#include "msgpack.hpp"
#include "rpccommon.h"

namespace dodo
{
    using namespace std;
    using namespace msgpack;
    
    template<int SIZE, typename ...Args>
    struct Eval;
    
    template<typename T>
    struct HelpEval
    {
        template<typename ...Args>
        struct Fuck
        {
            template<typename T, typename ...LeftArgs, typename ...NowArgs>
            static  void    eval(VariadicArgFunctor<Args...>* pThis, const char* buffer, size_t size, size_t& off, NowArgs&&... args)
            {
                typedef typename std::tuple_element<sizeof...(Args)-sizeof...(LeftArgs)-1, decltype(pThis->mTuple)>::type ARGTYPE;
                //static_assert(std::is_same<T, ARGTYPE>::value, "");

                auto& value = std::get<sizeof...(Args)-sizeof...(LeftArgs)-1>(pThis->mTuple);
                clear(value);
                
                msgpack::unpacked result;
                unpack(result, buffer, size, off);
                const msgpack::object& o = result.get();
                o.convert(&value);
                
                Eval<sizeof...(LeftArgs), Args...>::template eval<LeftArgs...>(pThis, buffer, size, off, args..., value);
            }
        };
    };
    
    template<>
    struct HelpEval < RpcRequestInfo >
    {
        template<typename ...Args>
        struct Fuck
        {
            template<typename T, typename ...LeftArgs, typename ...NowArgs>
            static  void    eval(VariadicArgFunctor<Args...>* pThis, const char* buffer, size_t size, size_t& off, NowArgs&&... args)
            {
                static_assert(std::is_same<T, RpcRequestInfo>::value, "");

                RpcRequestInfo value;
                value.setRequestID(pThis->getRequestID());
                Eval<sizeof...(LeftArgs), Args...>::template eval<LeftArgs...>(pThis, buffer, size, off, args..., value);
            }
        };
    };
    
    template<int SIZE, typename ...Args>
    struct Eval
    {
        template<typename ...LeftArgs, typename ...NowArgs>
        static  void    eval(VariadicArgFunctor<Args...>* pThis, const char* buffer, size_t size, size_t& off, NowArgs&&... args)
        {
            typedef HelpEval<typename std::tuple_element<sizeof...(Args)-sizeof...(LeftArgs), decltype(pThis->mTuple)>::type> TMP1;
            typedef typename TMP1::template Fuck<Args...> TMP;
            TMP::template eval<LeftArgs...>(pThis, buffer, size, off, args...);
        }
    };
    
    template<typename ...Args>
    struct Eval < 0, Args... >
    {
        template<typename ...NowArgs>
        static  void    eval(VariadicArgFunctor<Args...>* pThis, const char* buffer, size_t size, size_t& off, NowArgs&&... args)
        {
            (pThis->mf)(args...);
        }
    };
    
    struct Decode
    {
        template<typename ...Args>
        struct Invoke
        {
        public:
            static void invoke(int id, void* pvoid, const char* buffer, size_t len, size_t& off)
            {
                auto pThis = (VariadicArgFunctor<Args...>*)pvoid;
                pThis->setRequestID(id);
                Eval<sizeof...(Args), Args...>::template eval<Args...>(pThis, buffer, len, off);
            }
        };
    };
    
    class FunctionMgr : public BaseFunctorMgr < decltype(&Decode::Invoke<void>::invoke), Decode >
    {
    public:
        void    execute(const char* str, size_t size)
        {
            try
            {
                string name;
                std::size_t off = 0;
                
                {
                    int req_id;
                    msgpack::unpacked result;
                    unpack(result, str, size, off);
                    const msgpack::object& o = result.get();
                    o.convert(&req_id);
                    setRequestID(req_id);
                }

                {
                    msgpack::unpacked result;
                    unpack(result, str, size, off);
                    msgpack::object o = result.get();
                    o.convert(&name);
                }
                
                auto it = mWrapFunctions.find(name);
                assert(it != mWrapFunctions.end());
                if (it != mWrapFunctions.end())
                {
                    ((*it).second)(getRequestID(), mRealFunctionPtr[name], str, size, off);
                }
            }
            catch (msgpack::type_error)
            {
            }
            catch (...)
            {
            }
        }
    };

    template<bool>
    struct SelectWriteArgMsgpack
    {
        template<typename ARGTYPE>
        static  void    Write(BaseCaller& caller, FunctionMgr& functionMgr, msgpack::sbuffer& sbuf, msgpack::sbuffer& lambdabuf, const ARGTYPE& arg)
        {
            int id = caller.makeNextID();
            functionMgr.insertLambda(std::to_string(id), arg);
            msgpack::pack(&lambdabuf, id);
        }
    };
    
    template<>
    struct SelectWriteArgMsgpack < false >
    {
        template<typename ARGTYPE>
        static  void    Write(BaseCaller& caller, FunctionMgr& functionMgr, msgpack::sbuffer& sbuf, msgpack::sbuffer& lambdabuf, const ARGTYPE& arg)
        {
            msgpack::pack(&lambdabuf, -1);
            msgpack::pack(&sbuf, arg);
        }
    };

    class Caller : public BaseCaller
    {
    public:
        template<typename... Args>
        string    call(FunctionMgr& msgpackFunctionResponseMgr, const char* funname, const Args&... args)
        {
            msgpack::sbuffer lambdabuf;
            msgpack::sbuffer sbuf;
            msgpack::pack(&sbuf, funname);
            
            writeCallArg(msgpackFunctionResponseMgr, sbuf, lambdabuf, args...);
            
            return string(lambdabuf.data(), lambdabuf.size()) + string(sbuf.data(), sbuf.size());
        }
    private:
        template<typename Arg>
        void    writeCallArg(FunctionMgr& msgpackFunctionResponseMgr, msgpack::sbuffer& sbuf, msgpack::sbuffer& lambdabuf, const Arg& arg)
        {
            /*只(剩)有一个参数,肯定也为最后一个参数，允许为lambda*/
            SelectWriteArgMsgpack<HasCallOperator<Arg>::value>::Write(*this, msgpackFunctionResponseMgr, sbuf, lambdabuf, arg);
        }

        template<typename Arg1, typename... Args>
        void    writeCallArg(FunctionMgr& msgpackFunctionResponseMgr, msgpack::sbuffer& sbuf, msgpack::sbuffer& lambdabuf, const Arg1& arg1, const Args&... args)
        {
            msgpack::pack(&sbuf, arg1);
            writeCallArg(msgpackFunctionResponseMgr, sbuf, lambdabuf, args...);
        }

        void    writeCallArg(FunctionMgr& msgpackFunctionResponseMgr, msgpack::sbuffer& sbuf, msgpack::sbuffer& lambdabuf)
        {
            msgpack::pack(&lambdabuf, -1);
        }
    };
    
    struct MsgpackProtocol
    {
        typedef FunctionMgr FunctionMgr;
        typedef Caller Caller;
        
        template<class... Args>
        using tuple = msgpack::type::tuple < Args... > ;
        
        template<class... _Types> inline
        tuple<_Types...>
        static make_tuple(_Types&&... _Args)
        {
            return msgpack::type::make_tuple<_Types...>(std::forward<_Types>(_Args)...);
        }
    };
}

#endif