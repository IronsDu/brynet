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
            template<typename ...LeftArgs, typename ...NowArgs>
            static  void    eval(VariadicArgFunctor<Args...>* pThis, const char* buffer, size_t size, size_t& off, NowArgs&&... args)
            {
                /*  msgpack协议的call是逆序写入参数，所以这里逆序读取参数    */
                auto& value = std::get<sizeof...(LeftArgs)>(pThis->mTuple);
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
            template<typename ...LeftArgs, typename ...NowArgs>
            static  void    eval(VariadicArgFunctor<Args...>* pThis, const char* buffer, size_t size, size_t& off, NowArgs&&... args)
            {
                RpcRequestInfo value;
                value.setRequestID(pThis->getRequestID());
                Eval<sizeof...(LeftArgs), Args...>::template eval<LeftArgs...>(pThis, buffer, size, off, args..., value);
            }
        };
    };
    
    template<int SIZE, typename ...Args>
    struct Eval
    {
        template<typename T, typename ...LeftArgs, typename ...NowArgs>
        static  void    eval(VariadicArgFunctor<Args...>* pThis, const char* buffer, size_t size, size_t& off, NowArgs&&... args)
        {
            typedef HelpEval<typename std::tuple_element<sizeof...(LeftArgs), decltype(pThis->mTuple)>::type> TMP1;
            typedef typename TMP1::template Fuck<Args...> TMP;
            TMP::template eval<LeftArgs...>(pThis, buffer, size, off, args...);
        }
    };
    
    /*  http://stackoverflow.com/questions/15904288/how-to-reverse-the-order-of-arguments-of-a-variadic-template-function   */
    template<class ...Tn>
    struct revert;
    
    template<>
    struct revert <>
    {
        template<typename ...Args>
        struct Fuck
        {
            template<class ...Un>
            static void apply(VariadicArgFunctor<Args...>* pThis, Un const&... un)
            {
                (pThis->mf)(un...);
            }
        };
    };
    // recursion
    template<class T, class ...Tn>
    struct revert < T, Tn... >
    {
        template<typename ...Args>
        struct Fuck
        {
            template<class ...Un>
            static void apply(VariadicArgFunctor<Args...>* pThis, T const& t, Tn const&... tn, Un const&... un)
            {
                revert<Tn...>::template Fuck<Args...>::apply(pThis, tn..., t, un...);
            }
        };
    };
    
    template<typename ...Args>
    struct Eval < 0, Args... >
    {
        template<typename ...NowArgs>
        static  void    eval(VariadicArgFunctor<Args...>* pThis, const char* buffer, size_t size, size_t& off, NowArgs&&... args)
        {
            /*  因为实际参数是逆序的，所以要经过反转之后再回调处理函数 */
            revertArgsCallback(pThis, args...);
        }
        
        template<class A, class ...An>
        static void revertArgsCallback(VariadicArgFunctor<Args...>* pThis, A const& a, An const&... an)
        {
            revert<An...>::template Fuck<Args...>::apply(pThis, an..., a);
        }
        
        static void revertArgsCallback(VariadicArgFunctor<Args...>* pThis)
        {
            (pThis->mf)();
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
                msgpack::unpacked result;
                std::size_t off = 0;
                
                unpack(result, str, size, off);
                msgpack::object o = result.get();
                string name;
                o.convert(&name);
                
                {
                    int req_id;
                    msgpack::unpacked result;
                    unpack(result, str, size, off);
                    const msgpack::object& o = result.get();
                    o.convert(&req_id);
                    setRequestID(req_id);
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
        static  void    Write(BaseCaller& caller, FunctionMgr& functionMgr, msgpack::sbuffer& sbuf, const ARGTYPE& arg)
        {
            int id = caller.makeNextID();
            functionMgr.insertLambda(std::to_string(id), arg);
            msgpack::pack(&sbuf, id);
        }
    };
    
    template<>
    struct SelectWriteArgMsgpack < false >
    {
        template<typename ARGTYPE>
        static  void    Write(BaseCaller& caller, FunctionMgr& functionMgr, msgpack::sbuffer& sbuf, const ARGTYPE& arg)
        {
            msgpack::pack(&sbuf, -1);
            msgpack::pack(&sbuf, arg);
        }
    };
    
    class Caller : public BaseCaller
    {
    public:
        template<typename... Args>
        string    call(FunctionMgr& msgpackFunctionResponseMgr, const char* funname, const Args&... args)
        {
            msgpack::sbuffer sbuf;
            msgpack::pack(&sbuf, funname);
            
            writeCallArg(msgpackFunctionResponseMgr, sbuf, args...);
            
            return string(sbuf.data(), sbuf.size());
        }
    private:
        void    writeCallArg(FunctionMgr& msgpackFunctionResponseMgr, msgpack::sbuffer& sbuf, int& index){}
        
        template<typename Arg>
        void    writeCallArg(FunctionMgr& msgpackFunctionResponseMgr, msgpack::sbuffer& sbuf, const Arg& arg)
        {
            /*只(剩)有一个参数,肯定也为最后一个参数，允许为lambda*/
            SelectWriteArgMsgpack<HasCallOperator<Arg>::value>::Write(*this, msgpackFunctionResponseMgr, sbuf, arg);
        }
        
        template<typename Arg1, typename... Args>
        void    writeCallArg(FunctionMgr& msgpackFunctionResponseMgr, msgpack::sbuffer& sbuf, const Arg1& arg1, const Args&... args)
        {
            writeCallArg(msgpackFunctionResponseMgr, sbuf, args...);
            msgpack::pack(&sbuf, arg1);
        }
        
        void    writeCallArg(FunctionMgr& msgpackFunctionResponseMgr, msgpack::sbuffer& sbuf)
        {
            msgpack::pack(&sbuf, -1);
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