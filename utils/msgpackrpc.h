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
    
    template<class Tuple, std::size_t N>
    struct TupleRead
    {
        static void read(const char* buffer, size_t size, size_t& off, Tuple& t)
        {
            TupleRead<Tuple, N - 1>::read(buffer, size, off, t);
            ValueRead::read(buffer, size, off, std::get<N - 1>(t));
        }
    };

    template<class Tuple>
    struct TupleRead < Tuple, 1 >
    {
        static void read(const char* buffer, size_t size, size_t& off, Tuple& t)
        {
            ValueRead::read(buffer, size, off, std::get<0>(t));
        }
    };

    struct ValueRead
    {
        template<class... Args>
        static  void    read(const char* buffer, size_t size, size_t& off, std::tuple<Args...>& value)
        {
            TupleRead<decltype(value), sizeof...(Args)>::read(buffer, size, off, value);
        }

        template<typename T>
        static  void    read(const char* buffer, size_t size, size_t& off, T& value)
        {
            msgpack::unpacked result;
            msgpack::unpack(result, buffer, size, off);
            const msgpack::object& o = result.get();
            o.convert(&value);
        }

        template<typename T>
        static void     read(const char* buffer, size_t size, size_t& off, std::vector<T>& value)
        {
            int32_t len;
            read(buffer, size, off, len);
            while (off != size && len > 0)
            {
                T t;
                read(buffer, size, off, t);
                value.push_back(std::move(t));
                len--;
            }
        }

        template<typename K, typename T>
        static void     read(const char* buffer, size_t size, size_t& off, std::map<K , T>& value)
        {
            int32_t len;
            read(buffer, size, off, len);
            while (off != size && len > 0)
            {
                K key;
                read(buffer, size, off, key);
                T t;
                read(buffer, size, off, t);
                value.insert(std::make_pair(key, std::move(t)));
                len--;
            }
        }

    };

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
                static_assert(std::is_same<std::remove_const<std::remove_reference<T>::type>::type, ARGTYPE>::value, "");

                auto& value = std::get<sizeof...(Args)-sizeof...(LeftArgs)-1>(pThis->mTuple);
                clear(value);
                
                ValueRead::read(buffer, size, off, value);
                
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
                static_assert(std::is_same<std::remove_const<std::remove_reference<T>::type>::type, RpcRequestInfo>::value, "");

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
                cout << "parse error" << endl;
            }
            catch (...)
            {
                cout << "parse error" << endl;
            }
        }
    };

    template<class Tuple, std::size_t N>
    struct TupleWrite
    {
        static void write(msgpack::sbuffer& sbuf, const Tuple& value)
        {
            TupleWrite<Tuple, N - 1>::write(sbuf, value);
            ValueWrite::write(sbuf, std::get<N - 1>(value));
        }
    };

    template<class Tuple>
    struct TupleWrite < Tuple, 1 >
    {
        static void write(msgpack::sbuffer& sbuf, const Tuple& value)
        {
            ValueWrite::write(sbuf, std::get<0>(value));
        }
    };

    struct ValueWrite
    {
        template<typename T>
        static  void    write(msgpack::sbuffer& sbuf, const T& value)
        {
            msgpack::pack(&sbuf, value);
        }

        template<class... Args>
        static  void    write(msgpack::sbuffer& sbuf, const std::tuple<Args...>& value)
        {
            TupleWrite<decltype(value), sizeof...(Args)>::write(sbuf, value);
        }

        template<typename T>
        static  void    write(msgpack::sbuffer& sbuf, const vector<T>& value)
        {
            write(sbuf, (int32_t)value.size());
            for (auto& v : value)
            {
                write(sbuf, v);
            }
        }

        template<typename K, typename T>
        static  void    write(msgpack::sbuffer& sbuf, const map<K, T>& value)
        {
            write(sbuf, (int32_t)value.size());
            for (auto& v : value)
            {
                write(sbuf, v.first);
                write(sbuf, v.second);
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
            ValueWrite::write(sbuf, arg);
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
            ValueWrite::write(sbuf, arg1);
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
    };
}

#endif