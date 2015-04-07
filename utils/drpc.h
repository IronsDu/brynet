#ifndef _DRPC_H
#define _DRPC_H

#include <stdio.h>
#include <assert.h>
#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <functional>
#include <tuple>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
using namespace rapidjson;

#include "msgpack.hpp"
using namespace msgpack;

using namespace std;

namespace dodo
{
    template <typename T>
    class HasCallOperator
    {
        typedef char _One;
        typedef struct{ char a[2]; }_Two;
        template<typename T>
        static _One hasFunc(decltype(&T::operator()));
        template<typename T>
        static _Two hasFunc(...);
    public:
        static const bool value = sizeof(hasFunc<T>(nullptr)) == sizeof(_One);
    };

    class Utils
    {
    public:
        /*  反序列化    */
        static  void    readJson(const Value& msg, char& ret)
        {
            ret = msg.GetInt();
        }

        static  void    readJson(const Value& msg, int& ret)
        {
            ret = msg.GetInt();
        }

        static  void    readJson(const Value& msg, string& ret)
        {
            ret = msg.GetString();
        }

        static  void    readJson(const Value& msg, vector<int>& ret)
        {
            for (size_t i = 0; i < msg.Size(); ++i)
            {
                ret.push_back(msg[i].GetInt());
            }
        }

        static  void    readJson(const Value& msg, vector<string>& ret)
        {
            for (size_t i = 0; i < msg.Size(); ++i)
            {
                ret.push_back(msg[i].GetString());
            }
        }

        template<typename T>
        static  void    readJson(const Value& msg, vector<T>& ret)
        {
            for (size_t i = 0; i < msg.Size(); ++i)
            {
                T tmp;
                readJson(msg[i], tmp);
                ret.push_back(std::move(tmp));
            }
        }

        static  void    readJson(const Value& msg, map<string, string>& ret)
        {
            for (Value::ConstMemberIterator itr = msg.MemberBegin(); itr != msg.MemberEnd(); ++itr)
            {
                ret[(*itr).name.GetString()] = (*itr).value.GetString();
            }
        }

        static  void    readJson(const Value& msg, map<int, int>& ret)
        {
            for (Value::ConstMemberIterator itr = msg.MemberBegin(); itr != msg.MemberEnd(); ++itr)
            {
                ret[atoi((*itr).name.GetString())] = (*itr).value.GetInt();
            }
        }

        static  void    readJson(const Value& msg, map<string, int>& ret)
        {
            for (Value::ConstMemberIterator itr = msg.MemberBegin(); itr != msg.MemberEnd(); ++itr)
            {
                ret[(*itr).name.GetString()] = (*itr).value.GetInt();
            }
        }

        template<typename T>
        static  void    readJson(const Value& msg, map<string, T>& ret)
        {
            for (Value::ConstMemberIterator itr = msg.MemberBegin(); itr != msg.MemberEnd(); ++itr)
            {
                T tmp;
                readJson((*itr).value, tmp);
                ret[(*itr).name.GetString()] = std::move(tmp);
            }
        }

        template<typename T>
        static  void    readJson(const Value& msg, map<int, T>& ret)
        {
            for (Value::ConstMemberIterator itr = msg.MemberBegin(); itr != msg.MemberEnd(); ++itr)
            {
                T tmp;
                readJson((*itr).value, tmp);
                ret[atoi((*itr).name.GetString())] = std::move(tmp);
            }
        }

        /*  序列化-把数据转换为Json对象  */
        static  Value    writeJson(Document& doc, const int& value)
        {
            return Value(value);
        }

        static  Value   writeJson(Document& doc, const char* const& value)
        {
            return Value(value, doc.GetAllocator());
        }

        static  Value   writeJson(Document& doc, const string& value)
        {
            return Value(value.c_str(), doc.GetAllocator());
        }

        static  Value   writeJson(Document& doc, const vector<int>& value)
        {
            Value arrayObject(kArrayType);
            for (size_t i = 0; i < value.size(); ++i)
            {
                arrayObject.PushBack(Value(value[i]), doc.GetAllocator());
            }
            return arrayObject;
        }

        static  Value   writeJson(Document& doc, const vector<string>& value)
        {
            Value arrayObject(kArrayType);
            for (size_t i = 0; i < value.size(); ++i)
            {
                arrayObject.PushBack(Value(value[i].c_str(), doc.GetAllocator()), doc.GetAllocator());
            }
            return arrayObject;
        }

        template<typename T>
        static  Value   writeJson(Document& doc, const vector<T>& value)
        {
            Value arrayObject(kArrayType);
            for (size_t i = 0; i < value.size(); ++i)
            {
                arrayObject.PushBack(writeJson(doc, value[i]), doc.GetAllocator());
            }
            return arrayObject;
        }

        static  Value   writeJson(Document& doc, const map<string, string>& value)
        {
            Value mapObject(kObjectType);
            map<string, string>::const_iterator itend = value.end();
            for (map<string, string>::const_iterator it = value.begin(); it != itend; ++it)
            {
                mapObject.AddMember(GenericValue<UTF8<>>(it->first.c_str(), doc.GetAllocator()), Value(it->second.c_str(), doc.GetAllocator()), doc.GetAllocator());
            }
            return mapObject;
        }

        static  Value   writeJson(Document& doc, const map<int, string>& value)
        {
            Value mapObject(kObjectType);
            map<int, string>::const_iterator itend = value.end();
            for (map<int, string>::const_iterator it = value.begin(); it != itend; ++it)
            {
                mapObject.AddMember(GenericValue<UTF8<>>(std::to_string((*it).first).c_str(), doc.GetAllocator()), Value(it->second.c_str(), doc.GetAllocator()), doc.GetAllocator());
            }
            return mapObject;
        }

        static  Value   writeJson(Document& doc, const map<string, int>& value)
        {
            Value mapObject(kObjectType);
            map<string, int>::const_iterator itend = value.end();
            for (map<string, int>::const_iterator it = value.begin(); it != itend; ++it)
            {
                mapObject.AddMember(GenericValue<UTF8<>>(it->first.c_str(), doc.GetAllocator()), Value(it->second), doc.GetAllocator());
            }
            return mapObject;
        }

        template< typename V>
        static  Value   writeJson(Document& doc, const map<int, V>& value)
        {
            Value mapObject(kObjectType);
            /*遍历此map*/
            for (map<int, V>::const_iterator it = value.begin(); it != value.end(); ++it)
            {
                /*把value序列化到map的jsonobject中,key就是它在map结构中的key*/
                mapObject.AddMember(GenericValue<UTF8<>>(std::to_string(it->first).c_str(), doc.GetAllocator()), writeJson(doc, it->second), doc.GetAllocator());
            }

            /*把此map添加到msg中*/
            return mapObject;
        }

        template<typename V>
        static  Value   writeJson(Document& doc, const map<string, V>& value)
        {
            Value mapObject(kObjectType);
            /*遍历此map*/
            for (map<string, V>::const_iterator it = value.begin(); it != value.end(); ++it)
            {
                /*把value序列化到map的jsonobject中,key就是它在map结构中的key*/
                mapObject.AddMember(GenericValue<UTF8<>>(it->first.c_str(), doc.GetAllocator()), writeJson(doc, it->second), doc.GetAllocator());
            }

            /*把此map添加到msg中*/
            return mapObject;
        }

        template<typename T>
        static  void    writeJsonByIndex(Document& doc, Value& msg, const T& t, int index)
        {
            msg.AddMember(GenericValue<UTF8<>>(std::to_string(index).c_str(), doc.GetAllocator()), writeJson(doc, t), doc.GetAllocator());
        }
    };

    template<typename T>
    static void    clear(map<int, T>& v)
    {
        v.clear();
    }

    template<typename T>
    static void    clear(map<string, T>& v)
    {
        v.clear();
    }

    template<typename T>
    static void    clear(vector<T>& v)
    {
        v.clear();
    }

    template<typename ...Args>
    static void    clear(Args&... args)
    {
    }

    template<typename ...Args>
    struct VariadicArgFunctor
    {
        VariadicArgFunctor(std::function<void(Args...)> f)
        {
            mf = f;
            mRequestID = -1;
        }

        void    setRequestID(int id)
        {
            mRequestID = id;
        }

        int     getRequestID() const
        {
            return mRequestID;
        }

        std::function<void(Args...)>   mf;
        std::tuple<typename std::remove_const<typename std::remove_reference<Args>::type>::type...>  mTuple;    /*回调函数所需要的参数列表*/
        int     mRequestID;
    };

    template<typename CALLBACK_TYPE, typename INVOKE_TYPE>
    class BaseFunctorMgr
    {
    public:
        BaseFunctorMgr()
        {
            mRequestID = -1;
        }

        virtual ~BaseFunctorMgr()
        {
            for (auto& p : mRealFunctionPtr)
            {
                delete p.second;
            }
            mRealFunctionPtr.clear();
        }

        template<typename T>
        void insertLambda(string name, T lambdaObj)
        {
            _insertLambda<T>(name, lambdaObj, &T::operator());
        }

        template<typename ...Args>
        void insertStaticFunction(string name, void(*func)(Args...))
        {
            void* pbase = new VariadicArgFunctor<Args...>(func);
            assert(mWrapFunctions.find(name) == mWrapFunctions.end());
            mWrapFunctions[name] = INVOKE_TYPE::Invoke<Args...>::invoke;
            mRealFunctionPtr[name] = pbase;
        }

        void    del(const string& name)
        {
            mWrapFunctions.erase(name);
            auto it = mRealFunctionPtr.find(name);
            if (it != mRealFunctionPtr.end())
            {
                delete (*it).second;
            }
        }

        void    setRequestID(int id)
        {
            mRequestID = id;
        }

        int     getRequestID() const
        {
            return mRequestID;
        }
    private:
        template<typename LAMBDA_OBJ_TYPE, typename ...Args>
        void _insertLambda(string name, LAMBDA_OBJ_TYPE obj, void(LAMBDA_OBJ_TYPE::*func)(Args...) const)
        {
            void* pbase = new VariadicArgFunctor<Args...>(obj);
            assert(mWrapFunctions.find(name) == mWrapFunctions.end());
            mWrapFunctions[name] = INVOKE_TYPE::Invoke<Args...>::invoke;
            mRealFunctionPtr[name] = pbase;
        }

    protected:
        map<string, typename CALLBACK_TYPE>         mWrapFunctions;
        map<string, void*>                          mRealFunctionPtr;
        int                                         mRequestID;
    };

    class BaseCaller
    {
    public:
        BaseCaller() : mNextID(0){}
        int     makeNextID()
        {
            mNextID++;
            return mNextID;
        }

        int     getNowID() const
        {
            return mNextID;
        }
    private:
        int         mNextID;
    };

    class RpcRequestInfo
    {
    public:
        RpcRequestInfo()
        {
            mRequestID = -1;
        }

        void    setRequestID(int id)
        {
            mRequestID = id;
        }

        int     getRequestID() const
        {
            return mRequestID;
        }
    private:
        int     mRequestID;
    };

    struct JsonProtocol
    {
        struct Decode
        {
            template<typename ...Args>
            struct Invoke
            {
            public:
                static void invoke(int id, void* pvoid, const Value& msg)
                {
                    auto pThis = (VariadicArgFunctor<Args...>*)pvoid;
                    pThis->setRequestID(id);
                    int parmIndex = 0;
                    Eval<sizeof...(Args), Args...>::eval<Args...>(pThis, msg, parmIndex);
                }
            };
        };

        template<int SIZE, typename ...Args>
        struct Eval
        {
            template<typename T>
            struct HelpEval
            {
                template<typename ...LeftArgs, typename ...NowArgs>
                static  void    eval(VariadicArgFunctor<Args...>* pThis, const Value& msg, int& parmIndex, NowArgs&&... args)
                {
                    const Value& element = msg[std::to_string(parmIndex++).c_str()];

                    auto& value = std::get<sizeof...(Args)-sizeof...(LeftArgs)-1>(pThis->mTuple);
                    clear(value);
                    Utils::readJson(element, value);

                    Eval<sizeof...(LeftArgs), Args...>::eval<LeftArgs...>(pThis, msg, parmIndex, args..., value);
                }
            };

            template<>
            struct HelpEval < RpcRequestInfo >
            {
                template<typename ...LeftArgs, typename ...NowArgs>
                static  void    eval(VariadicArgFunctor<Args...>* pThis, const Value& msg, int& parmIndex, NowArgs&&... args)
                {
                    RpcRequestInfo value;
                    value.setRequestID(pThis->getRequestID());
                    Eval<sizeof...(LeftArgs), Args...>::eval<LeftArgs...>(pThis, msg, parmIndex, args..., value);
                }
            };

            template<typename T, typename ...LeftArgs, typename ...NowArgs>
            static  void    eval(VariadicArgFunctor<Args...>* pThis, const Value& msg, int& parmIndex, NowArgs&&... args)
            {
                HelpEval<T>::eval<LeftArgs...>(pThis, msg, parmIndex, args...);
            }
        };

        template<typename ...Args>
        struct Eval<0, Args...>
        {
            template<typename ...NowArgs>
            static  void    eval(VariadicArgFunctor<Args...>* pThis, const Value& msg, int& parmIndex, NowArgs&&... args)
            {
                (pThis->mf)(args...);
            }
        };

        class FunctionMgr : public BaseFunctorMgr<decltype(&Decode::Invoke<void>::invoke), Decode>
        {
        public:
            void    execute(const char* str, size_t size)
            {
                mDoc.Parse(str);

                string name = mDoc["name"].GetString();
                setRequestID(mDoc["req_id"].GetInt());
                const Value& parmObject = mDoc["parm"];

                auto it = mWrapFunctions.find(name);
                assert(it != mWrapFunctions.end());
                if (it != mWrapFunctions.end())
                {
                    ((*it).second)(getRequestID(), mRealFunctionPtr[name], parmObject);
                }
            }
        private:
            Document                    mDoc;
        };

        class Caller : public BaseCaller
        {
        public:
            Caller() : mWriter(mBuffer){}

            template<typename... Args>
            string    call(FunctionMgr& jsonFunctionResponseMgr, const char* funname, const Args&... args)
            {
                int old_req_id = getNowID();

                Value msg(kObjectType);
                msg.AddMember(GenericValue<UTF8<>>("name", mDoc.GetAllocator()), Value(funname, mDoc.GetAllocator()), mDoc.GetAllocator());
                int index = 0;

                Value parms(kObjectType);
                writeCallArg(jsonFunctionResponseMgr, mDoc, parms, index, args...);
                msg.AddMember(GenericValue<UTF8<>>("parm", mDoc.GetAllocator()), parms, mDoc.GetAllocator());

                int now_req_id = getNowID();
                /*req_id表示调用方的请求id，服务器(rpc被调用方)通过此id返回消息(返回值)给调用方*/
                msg.AddMember(GenericValue<UTF8<>>("req_id", mDoc.GetAllocator()), Value(old_req_id == now_req_id ? -1 : now_req_id), mDoc.GetAllocator());

                mBuffer.Clear();
                mWriter.Reset(mBuffer);
                msg.Accept(mWriter);
                return mBuffer.GetString();
            }
        private:
            void    writeCallArg(FunctionMgr& jsonFunctionResponseMgr, Document& doc, Value&, int& index){}

            template<typename Arg>
            void    writeCallArg(FunctionMgr& jsonFunctionResponseMgr, Document& doc, Value& msg, int& index, const Arg& arg)
            {
                /*只(剩)有一个参数,肯定也为最后一个参数，允许为lambda*/
                SelectWriteArgJson<HasCallOperator<Arg>::value>::Write(*this, jsonFunctionResponseMgr, doc, msg, arg, index++);
            }

            template<typename Arg1, typename... Args>
            void    writeCallArg(FunctionMgr& jsonFunctionResponseMgr, Document& doc, Value& msg, int& index, const Arg1& arg1, const Args&... args)
            {
                Utils::writeJsonByIndex(doc, msg, arg1, index++);
                writeCallArg(jsonFunctionResponseMgr, doc, msg, index, args...);
            }

            template<bool>
            struct SelectWriteArgJson;

            template<>
            struct SelectWriteArgJson<true>
            {
                template<typename ARGTYPE>
                static  void    Write(Caller& jc, FunctionMgr& functionMgr, Document& doc, Value& parms, const ARGTYPE& arg, int index)
                {
                    int id = jc.makeNextID();
                    functionMgr.insertLambda(std::to_string(id), arg);
                }
            };

            template<>
            struct SelectWriteArgJson<false>
            {
                template<typename ARGTYPE>
                static  void    Write(Caller& jc, FunctionMgr& functionMgr, Document& doc, Value& parms, const ARGTYPE& arg, int index)
                {
                    Utils::writeJsonByIndex(doc, parms, arg, index);
                }
            };
        private:
            Document                    mDoc;
            StringBuffer                mBuffer;
            Writer<StringBuffer>        mWriter;
        };
    };

    struct MsgpackProtocol
    {
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
                    Eval<sizeof...(Args), Args...>::eval<Args...>(pThis, buffer, len, off);
                }
            };
        };

        template<int SIZE, typename ...Args>
        struct Eval
        {
            template<typename T>
            struct HelpEval
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

                    Eval<sizeof...(LeftArgs), Args...>::eval<LeftArgs...>(pThis, buffer, size, off, args..., value);
                }
            };

            template<>
            struct HelpEval < RpcRequestInfo >
            {
                template<typename ...LeftArgs, typename ...NowArgs>
                static  void    eval(VariadicArgFunctor<Args...>* pThis, const char* buffer, size_t size, size_t& off, NowArgs&&... args)
                {
                    RpcRequestInfo value;
                    value.setRequestID(pThis->getRequestID());
                    Eval<sizeof...(LeftArgs), Args...>::eval<LeftArgs...>(pThis, buffer, size, off, args..., value);
                }
            };

            template<typename T, typename ...LeftArgs, typename ...NowArgs>
            static  void    eval(VariadicArgFunctor<Args...>* pThis, const char* buffer, size_t size, size_t& off, NowArgs&&... args)
            {
                HelpEval<std::tuple_element<sizeof...(LeftArgs), decltype(pThis->mTuple)>::type>::eval<LeftArgs...>(pThis, buffer, size, off, args...);
            }
        };

        template<typename ...Args>
        struct Eval<0, Args...>
        {
            template<typename ...NowArgs>
            static  void    eval(VariadicArgFunctor<Args...>* pThis, const char* buffer, size_t size, size_t& off, NowArgs&&... args)
            {
                /*  因为实际参数是逆序的，所以要经过反转之后再回调处理函数 */
                revertArgsCallback(pThis, args...);
            }

            /*  http://stackoverflow.com/questions/15904288/how-to-reverse-the-order-of-arguments-of-a-variadic-template-function   */
            template<class ...Tn>
            struct revert;

            template<>
            struct revert<>
            {
                template<class ...Un>
                static void apply(VariadicArgFunctor<Args...>* pThis, Un const&... un)
                {
                    (pThis->mf)(un...);
                }
            };
            // recursion
            template<class T, class ...Tn>
            struct revert<T, Tn...>
            {
                template<class ...Un>
                static void apply(VariadicArgFunctor<Args...>* pThis, T const& t, Tn const&... tn, Un const&... un)
                {
                    revert<Tn...>::apply(pThis, tn..., t, un...);
                }
            };

            template<class A, class ...An>
            static void revertArgsCallback(VariadicArgFunctor<Args...>* pThis, A const& a, An const&... an)
            {
                revert<An...>::apply(pThis, an..., a);
            }

            static void revertArgsCallback(VariadicArgFunctor<Args...>* pThis)
            {
                (pThis->mf)();
            }
        };

        class FunctionMgr : public BaseFunctorMgr<decltype(&Decode::Invoke<void>::invoke), Decode>
        {
        public:
            void    execute(const char* str, size_t size)
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

            template<bool>
            struct SelectWriteArgMsgpack;

            template<>
            struct SelectWriteArgMsgpack<true>
            {
                template<typename ARGTYPE>
                static  void    Write(Caller& mc, FunctionMgr& functionMgr, msgpack::sbuffer& sbuf, const ARGTYPE& arg)
                {
                    int id = mc.makeNextID();
                    functionMgr.insertLambda(std::to_string(id), arg);
                    msgpack::pack(&sbuf, id);
                }
            };

            template<>
            struct SelectWriteArgMsgpack<false>
            {
                template<typename ARGTYPE>
                static  void    Write(Caller& mc, FunctionMgr& functionMgr, msgpack::sbuffer& sbuf, const ARGTYPE& arg)
                {
                    msgpack::pack(&sbuf, -1);
                    msgpack::pack(&sbuf, arg);
                }
            };
        };
    };

    template<typename PROTOCOL_TYPE = MsgpackProtocol>
    class rpc
    {
    public:
        rpc()
        {
            /*  注册rpc_reply 服务函数，处理rpc返回值   */
            def("rpc_reply", [this](int req_id, const string& response){
                handleResponse(response);
                mResponseCallbacks.del(std::to_string(req_id));
            });
        }

        template<typename F>
        void        def(const char* funname, F func)
        {
            regFunctor(funname, func);
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
        void    handleRpc(const string&& str)
        {
            handleRpc(str);
        }

        /*  返回数据给RPC调用端    */
        template<typename... Args>
        string    reply(int reqid, const Args&... args)
        {
            /*  把实际返回值打包作为参数,调用对端的rpc_reply 函数*/
            return call("rpc_reply", reqid, call(std::to_string(reqid).c_str(), args...));
        }

        /*  调用方处理收到的rpc返回值(消息)*/
        void    handleResponse(const string& str)
        {
            mResponseCallbacks.execute(str.c_str(), str.size());
        }
        void    handleResponse(const string&& str)
        {
            handleResponse(str);
        }
    private:
        template<typename ...Args>
        void regFunctor(const char* funname, void(*func)(Args...))
        {
            mRpcFunctions.insertStaticFunction(funname, func);
        }

        template<typename LAMBDA>
        void regFunctor(const char* funname, LAMBDA lambdaObj)
        {
            mRpcFunctions.insertLambda(funname, lambdaObj);
        }
    private:
        typename PROTOCOL_TYPE::FunctionMgr      mResponseCallbacks;
        typename PROTOCOL_TYPE::FunctionMgr      mRpcFunctions;
        typename PROTOCOL_TYPE::Caller           mCaller;
    };
}

#endif