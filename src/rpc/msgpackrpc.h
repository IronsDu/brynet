#ifndef _MSGPACKRPC_H
#define _MSGPACKRPC_H

#include <assert.h>
#include <string>
#include <iostream>
#include <map>
#include <vector>
#include <functional>
#include <tuple>

#include "msgpack.hpp"
#include "rpccommon.h"

namespace google
{
    namespace protobuf
    {
        class Message;
    }
}

namespace dodo
{
    using namespace std;
    using namespace msgpack;
    
    struct MsgpackProtocol
    {
        template<class Tuple, std::size_t N>
        struct TupleRead
        {
            static void read(const char* buffer, size_t size, size_t& off, Tuple& t)
            {
                TupleRead<Tuple, N - 1>::read(buffer, size, off, t);
                ValueRead<std::is_base_of<::google::protobuf::Message, std::remove_reference<decltype(std::get<N - 1>(t))>::type>::value>::read(buffer, size, off, std::get<N - 1>(t));
            }
        };

        template<class Tuple>
        struct TupleRead < Tuple, 1 >
        {
            static void read(const char* buffer, size_t size, size_t& off, Tuple& t)
            {
                ValueRead<std::is_base_of<::google::protobuf::Message, std::remove_reference<decltype(std::get<0>(t))>::type>::value>::read(buffer, size, off, std::get<0>(t));
            }
        };

        template<bool isPB>
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
            static void     read(const char* buffer, size_t size, size_t& off, std::map<K, T>& value)
            {
                int32_t len;
                read(buffer, size, off, len);
                while (off != size && len > 0)
                {
                    K key;
                    read(buffer, size, off, key);
                    T t;
                    read(buffer, size, off, t);
                    value.insert(std::make_pair(std::move(key), std::move(t)));
                    len--;
                }
            }

        };

        template<>
        struct ValueRead<true>
        {
            template<typename T>
            static  void    read(const char* buffer, size_t size, size_t& off, T& value)
            {
                /*TODO::直接读取二进制流*/
                string str;
                msgpack::unpacked result;
                msgpack::unpack(result, buffer, size, off);
                const msgpack::object& o = result.get();
                o.convert(&str);
                value.ParseFromArray(str.c_str(), str.size());
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

                    ValueRead<std::is_base_of<::google::protobuf::Message, std::remove_reference<decltype(value)>::type>::value>::read(buffer, size, off, value);

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
                ValueWrite<std::is_base_of<::google::protobuf::Message, decltype(std::get<N - 1>(value))>::value>::write(sbuf, std::get<N - 1>(value));
            }
        };

        template<class Tuple>
        struct TupleWrite < Tuple, 1 >
        {
            static void write(msgpack::sbuffer& sbuf, const Tuple& value)
            {
                ValueWrite<std::is_base_of<::google::protobuf::Message, decltype(std::get<0>(value))>::value>::write(sbuf, std::get<0>(value));
            }
        };

        template<bool isPB>
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

        template<>
        struct ValueWrite<true>
        {
            template<typename T>
            static  void    write(msgpack::sbuffer& sbuf, const T& value)
            {
                char stackBuf[1024];
                int pbByteSize = value.ByteSize();
                if (pbByteSize <= sizeof(stackBuf))
                {
                    value.SerializeToArray(stackBuf, pbByteSize);

                    msgpack::packer<msgpack::sbuffer>(sbuf).pack_str(pbByteSize);
                    msgpack::packer<msgpack::sbuffer>(sbuf).pack_str_body(stackBuf, pbByteSize);
                }
                else
                {
                    string str;
                    str.resize(pbByteSize);
                    value.SerializeToArray((void*)str.c_str(), pbByteSize);

                    msgpack::packer<msgpack::sbuffer>(sbuf).pack_str(str.size());
                    msgpack::packer<msgpack::sbuffer>(sbuf).pack_str_body(str.c_str(), str.size());
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
                functionMgr.insertFunction(std::to_string(id), arg);
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
                ValueWrite<std::is_base_of<::google::protobuf::Message, ARGTYPE>::value>::write(sbuf, arg);
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
                SelectWriteArgMsgpack<std::is_function<Arg>::value>::Write(*this, msgpackFunctionResponseMgr, sbuf, lambdabuf, arg);
            }

            template<typename Arg1, typename... Args>
            void    writeCallArg(FunctionMgr& msgpackFunctionResponseMgr, msgpack::sbuffer& sbuf, msgpack::sbuffer& lambdabuf, const Arg1& arg1, const Args&... args)
            {
                ValueWrite<std::is_base_of<::google::protobuf::Message, Arg1>::value>::write(sbuf, arg1);
                writeCallArg(msgpackFunctionResponseMgr, sbuf, lambdabuf, args...);
            }

            void    writeCallArg(FunctionMgr& msgpackFunctionResponseMgr, msgpack::sbuffer& sbuf, msgpack::sbuffer& lambdabuf)
            {
                msgpack::pack(&lambdabuf, -1);
            }
        };
    };
}

#endif