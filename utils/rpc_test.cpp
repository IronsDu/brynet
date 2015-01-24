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

    template<int Size>
    struct SizeType
    {
        typedef int TYPE;
    };
    template<>
    struct SizeType<0>
    {
        typedef char TYPE;
    };

    template<typename A>
    struct remove_const { typedef A type; };
    template<typename A>
    struct remove_const<const A> { typedef A type; };

    template<typename A>
    struct base_type { typedef A type; };
    template<typename A>
    struct base_type<A*> { typedef A type; };
    template<typename A>
    struct base_type<A&> { typedef A type; };

    class Utils
    {
    public:
        /*  反序列化    */
        template<typename T>
        struct ReadJson;

        template<>
        struct ReadJson<char>
        {
            static  char    read(const Value& msg)
            {
                return msg.GetInt();
            }
        };

        template<>
        struct ReadJson<int>
        {
            static  int    read(const Value& msg)
            {
                return msg.GetInt();
            }
        };

        template<>
        struct ReadJson<string>
        {
            static  string    read(const Value& msg)
            {
                return msg.GetString();
            }
        };

        template<>
        struct ReadJson<vector<int>>
        {
            static  vector<int>    read(const Value& msg)
            {
                vector<int> ret;
                for (size_t i = 0; i < msg.Size(); ++i)
                {
                    ret.push_back(msg[i].GetInt());
                }

                return ret;
            }
        };

        template<>
        struct ReadJson<vector<string>>
        {
            static  vector<string>    read(const Value& msg)
            {
                vector<string> ret;
                for (size_t i = 0; i < msg.Size(); ++i)
                {
                    ret.push_back(msg[i].GetString());
                }

                return ret;
            }
        };

        template<typename T>
        struct ReadJson<vector<T>>
        {
            static  vector<T>    read(const Value& msg)
            {
                vector<T> ret;
                for (size_t i = 0; i < msg.Size(); ++i)
                {
                    ret.push_back(ReadJson<T>::read(msg[i]));
                }

                return ret;
            }
        };

        template<>
        struct ReadJson<map<string, string>>
        {
            static  map<string, string>    read(const Value& msg)
            {
                map<string, string> ret;
                for (Value::ConstMemberIterator itr = msg.MemberBegin(); itr != msg.MemberEnd(); ++itr)
                {
                    ret[(*itr).name.GetString()] = (*itr).value.GetString();
                }
                return ret;
            }
        };

        template<>
        class ReadJson<map<int, int>>
        {
            static  map<int, int>    read(const Value& msg)
            {
                map<int, int> ret;
                for (Value::ConstMemberIterator itr = msg.MemberBegin(); itr != msg.MemberEnd(); ++itr)
                {
                    ret[atoi((*itr).name.GetString())] = (*itr).value.GetInt();
                }
                return ret;
            }
        };

        template<>
        struct ReadJson<map<string, int>>
        {
            static  map<string, int>    read(const Value& msg)
            {
                map<string, int> ret;
                for (Value::ConstMemberIterator itr = msg.MemberBegin(); itr != msg.MemberEnd(); ++itr)
                {
                    ret[(*itr).name.GetString()] = (*itr).value.GetInt();
                }
                return ret;
            }
        };

        template<typename T>
        struct ReadJson<map<string, T>>
        {
            static  map<string, T>    read(const Value& msg)
            {
                map<string, T> ret;
                for (Value::ConstMemberIterator itr = msg.MemberBegin(); itr != msg.MemberEnd(); ++itr)
                {
                    ret[(*itr).name.GetString()] = ReadJson<T>::read((*itr).value);
                }
                return ret;
            }
        };

        template<typename T>
        struct ReadJson<map<int, T>>
        {
            static  map<int, T>    read(const Value& msg)
            {
                map<int, T> ret;
                for (Value::ConstMemberIterator itr = msg.MemberBegin(); itr != msg.MemberEnd(); ++itr)
                {
                    ret[atoi((*itr).name.GetString())] = ReadJson<T>::read((*itr).value);
                }
                return ret;
            }
        };

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

    class FunctionMgr
    {
    public:
        ~FunctionMgr()
        {
            for (auto& p : mRealFunctionPtr)
            {
                delete p.second;
            }
        }

        void    execute(const char* str)
        {
            mDoc.Parse(str);

            string name = mDoc["name"].GetString();
            const Value& parmObject = mDoc["parm"];

            map<string, pf_wrap>::iterator it = mWrapFunctions.find(name);
            assert(it != mWrapFunctions.end());
            if (it != mWrapFunctions.end())
            {
                ((*it).second)(mRealFunctionPtr[name], parmObject);
            }
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
            mWrapFunctions[name] = VariadicArgFunctor<Args...>::invoke;
            mRealFunctionPtr[name] = pbase;
        }

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
        template<typename ...Args>
        struct VariadicArgFunctor
        {
            VariadicArgFunctor(std::function<void(Args...)> f)
            {
                mf = f;
            }

            static void invoke(void* pvoid, const Value& msg)
            {
                int parmIndex = 0;
                eval<Args...>(SizeType<sizeof...(Args)>::TYPE(), pvoid, msg, parmIndex);
            }

            template<typename T, typename ...LeftArgs, typename ...NowArgs>
            static  void    eval(int _, void* pvoid, const Value& msg, int& parmIndex, NowArgs&&... args)
            {
                const Value& element = msg[std::to_string(parmIndex++).c_str()];
                eval<LeftArgs...>(SizeType<sizeof...(LeftArgs)>::TYPE(), pvoid, msg, parmIndex, args..., Utils::ReadJson<remove_const<base_type<T>::type>::type>::read(element));
            }

            template<typename ...NowArgs>
            static  void    eval(char _, void* pvoid, const Value& msg, int& parmIndex, NowArgs&&... args)
            {
                VariadicArgFunctor<Args...>* pthis = (VariadicArgFunctor<Args...>*)pvoid;
                (pthis->mf)(args...);
            }
        private:
            std::function<void(Args...)>   mf;
        };

        template<typename LAMBDA_OBJ_TYPE, typename ...Args>
        void _insertLambda(string name, LAMBDA_OBJ_TYPE obj, void(LAMBDA_OBJ_TYPE::*func)(Args...) const)
        {
            void* pbase = new VariadicArgFunctor<Args...>(obj);
            assert(mWrapFunctions.find(name) == mWrapFunctions.end());
            mWrapFunctions[name] = VariadicArgFunctor<Args...>::invoke;
            mRealFunctionPtr[name] = pbase;
        }

    private:
        typedef void(*pf_wrap)(void* pbase, const Value& msg);
        map<string, pf_wrap>        mWrapFunctions;
        map<string, void*>          mRealFunctionPtr;
        int                         mNextID;
        Document                    mDoc;
    };

    template<bool>
    struct SelectWriteArg;

    template<>
    struct SelectWriteArg<true>
    {
        template<typename ARGTYPE>
        static  void    Write(FunctionMgr& functionMgr, Document& doc, Value& parms, const ARGTYPE& arg, int index)
        {
            int id = functionMgr.makeNextID();
            functionMgr.insertLambda(std::to_string(id), arg);
        }
    };

    template<>
    struct SelectWriteArg<false>
    {
        template<typename ARGTYPE>
        static  void    Write(FunctionMgr& functionMgr, Document& doc, Value& parms, const ARGTYPE& arg, int index)
        {
            Utils::writeJsonByIndex(doc, parms, arg, index);
        }
    };

    class rpc
    {
    public:
        rpc() : mWriter(mBuffer)
        {
            /*  注册rpc_reply 服务函数，处理rpc返回值   */
            def("rpc_reply", [this](const string& response){
                handleResponse(response);
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
            int old_req_id = mResponseCallbacks.getNowID();

            Value msg(kObjectType);
            msg.AddMember(GenericValue<UTF8<>>("name", mDoc.GetAllocator()), Value(funname, mDoc.GetAllocator()), mDoc.GetAllocator());
            int index = 0;
            
            Value parms(kObjectType);
            writeCallArg(mDoc, parms, index, args...);
            msg.AddMember(GenericValue<UTF8<>>("parm", mDoc.GetAllocator()), parms, mDoc.GetAllocator());

            int now_req_id = mResponseCallbacks.getNowID();
            /*req_id表示调用方的请求id，服务器(rpc被调用方)通过此id返回消息(返回值)给调用方*/
            msg.AddMember(GenericValue<UTF8<>>("req_id", mDoc.GetAllocator()), Value(old_req_id == now_req_id ? -1 : now_req_id), mDoc.GetAllocator());

            mBuffer.Clear();
            mWriter.Reset(mBuffer);
            msg.Accept(mWriter);
            return mBuffer.GetString();
        }

        /*  处理rpc请求 */
        void    handleRpc(const string& str)
        {
            mRpcFunctions.execute(str.c_str());
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
            return call("rpc_reply", call(std::to_string(reqid).c_str(), args...));
        }

        /*  调用方处理收到的rpc返回值(消息)*/
        void    handleResponse(const string& str)
        {
            mResponseCallbacks.execute(str.c_str());
        }
        void    handleResponse(const string&& str)
        {
            handleResponse(str);
        }

    private:
        void    writeCallArg(Document& doc, int& index){}

        template<typename Arg>
        void    writeCallArg(Document& doc, Value& msg, int& index, const Arg& arg)
        {
            /*只(剩)有一个参数,肯定也为最后一个参数，允许为lambda*/
            _selectWriteArg(doc, msg, arg, index++);
        }

        template<typename Arg1, typename... Args>
        void    writeCallArg(Document& doc, Value& msg, int& index, const Arg1& arg1, const Args&... args)
        {
            Utils::writeJsonByIndex(doc, msg, arg1, index++);
            writeCallArg(doc, msg, index, args...);
        }
    private:

        /*如果是lambda则加入回调管理器，否则添加到rpc参数*/
        template<typename ARGTYPE>
        void    _selectWriteArg(Document& doc, Value& msg, const ARGTYPE& arg, int index)
        {
            SelectWriteArg<HasCallOperator<ARGTYPE>::value>::Write(mResponseCallbacks, doc, msg, arg, index);
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
        FunctionMgr                 mResponseCallbacks;
        FunctionMgr                 mRpcFunctions;
        Document                    mDoc;
        StringBuffer                mBuffer;
        Writer<StringBuffer>        mWriter;
    };
}

class Player : public dodo::rpc
{
public:
    Player()
    {
        registerHandle("player_attack", &Player::attack);
        registerHandle("player_hi", &Player::hi);
    }

private:
    template<typename... Args>
    void        registerHandle(string name, void (Player::*callback)(Args...))
    {
        def(name.c_str(), [this, callback](Args... args){
            (this->*callback)(args...);
        });
    }

private:
    void    attack(string target)
    {
        cout << "attack:" << target << endl;
    }

    void    hi(string i, string j)
    {
        cout << i << j << endl;
    }
};

void test1(int a, int b)
{
    cout << "in test1" << endl;
    cout << a << ", " << b << endl;
}

void test2(int a, int b, string c)
{
    cout << "in test2" << endl;
    cout << a << ", " << b << ", " << c << endl;
}

void test3(string a, int b, string c)
{
    cout << "in test3" << endl;
    cout << a << ", " << b << ", " << c << endl;
}

void test4(const string a, int b)
{
    cout << "in test4" << endl;
    cout << a << "," << b <<  endl;
}

void test5(const string a, int& b, const map<string, map<int, string>>& vlist)
{
}

void test6(string a, int b, map<string, int> vlist)
{
}

void test7(vector<map<int,string>>& vlist)
{
}
#ifdef _MSC_VER
#include <Windows.h>
#endif
int main()
{
    int upvalue = 10;
    using namespace dodo;

    Player rpc_server; /*rpc服务器*/
    Player rpc_client; /*rpc客户端*/

    string rpc_request_msg; /*  rpc消息   */
    string rpc_response_str;       /*  rpc返回值  */

    rpc_server.def("test4", test4);
    rpc_server.def("test5", test5);
    rpc_server.def("test7", test7);

    std::function<void(int)> functor = [](int i){
        //cout << "i is " << i << endl;
    };
    rpc_server.def("test_functor", functor);
    rpc_server.def("test_lambda", [](int j){
        cout << "j is " << j << endl;
    });
    
    int count = 0;
#ifdef _MSC_VER
#include <Windows.h>
    DWORD starttime = GetTickCount();
    while (count++ <= 100000)
    {
        rpc_request_msg = rpc_client.call("test_functor", 1);
        rpc_server.handleRpc(rpc_request_msg);
    }

    cout << "cost :" << GetTickCount() - starttime << endl;
#endif
    
    rpc_request_msg = rpc_client.call("test_lambda", 2);
    rpc_server.handleRpc(rpc_request_msg);

    rpc_request_msg = rpc_client.call("player_attack", "Li Lei");
    rpc_server.handleRpc(rpc_request_msg);
    rpc_request_msg = rpc_client.call("player_hi", "Hello", "World");
    rpc_server.handleRpc(rpc_request_msg);
    
    {
        vector<map<int, string>> vlist;
        map<int, string> a = { { 1, "dzw" } };
        map<int, string> b = { { 2, "haha" } };
        vlist.push_back(a);
        vlist.push_back(b);

        int count = 0;
#ifdef _MSC_VER
#include <Windows.h>
        DWORD starttime = GetTickCount();
        while (count++ <= 100000)
        {
            rpc_request_msg = rpc_client.call("test7", vlist);
            rpc_server.handleRpc(rpc_request_msg);
        }

        cout << "cost :" << GetTickCount() - starttime << endl;
#endif
    }

    map<int, string> m1;
    m1[1] = "Li";
    map<int, string> m2;
    m2[2] = "Deng";
    map<string, map<int, string>> mlist;
    mlist["100"] = m1;
    mlist["200"] = m2;

    {
        rpc_request_msg = rpc_client.call("test5", "a", 1, mlist, [&upvalue](int a, int b){
            upvalue++;
            cout << "upvalue:" << upvalue << ", a:" << a << ", b:" << b << endl;
        });

        rpc_server.handleRpc(rpc_request_msg);
    }

    {
        rpc_request_msg = rpc_client.call("test5", "a", 1, mlist, [&upvalue](string a, string b, int c){
            upvalue++;
            cout << "upvalue:" << upvalue << ", a:" << a << ", b:" << b << ", c:" << c << endl;
        });

        rpc_server.handleRpc(rpc_request_msg);
    }

    {
        rpc_request_msg = rpc_client.call("test4", "a", 1);
        rpc_server.handleRpc(rpc_request_msg);
    }

    /*  模拟服务器通过reply返回数据给rpc client,然后rpc client处理收到的rpc返回值 */
    {
        rpc_response_str = rpc_server.reply(1, 1, 2);   /* (1,1,2)中的1为调用方的req_id, (1,2)为返回值 */
        rpc_client.handleRpc(rpc_response_str);
    }

    {
        rpc_response_str = rpc_server.reply(2, "hello", "world", 3);
        rpc_client.handleRpc(rpc_response_str);
    }

    cin.get();

    return 0;
}