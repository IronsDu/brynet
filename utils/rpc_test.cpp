#include <stdio.h>
#include <assert.h>
#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <functional>
#include <tuple>

#include "json_object.h"

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

    class Utils
    {
    public:
        void static readJson(JsonObject& msg, const char* key, char& tmp)
        {
            tmp = msg.getInt(key);
        }

        void static readJson(JsonObject& msg, const char* key, int& tmp)
        {
            tmp = msg.getInt(key);
        }

        void static readJson(JsonObject& msg, const char* key, JsonObject& tmp)
        {
            tmp = msg.getObject(key);
        }

        void static readJson(JsonObject& msg, const char* key, string& tmp)
        {
            tmp = msg.getStr(key);
        }

        void static readJson(JsonObject& msg, const char* key, vector<int>& tmp)
        {
            vector<int> ret;
            JsonObject arrayJson = msg.getObject(key);
            for (int i = 0; i < arrayJson.getSize(); ++i)
            {
                stringstream ss;
                ss << i;
                JsonObject valueObject = arrayJson.getByIndex(i);
                ret.push_back(atoi(valueObject.toString().c_str()));
            }
            tmp = ret;
        }

        void static readJson(JsonObject& msg, const char* key, vector<string>& tmp)
        {
            vector<string> ret;
            JsonObject arrayJson = msg.getObject(key);
            for (int i = 0; i < arrayJson.getSize(); ++i)
            {
                stringstream ss;
                ss << i;
                JsonObject valueObject = arrayJson.getByIndex(i);
                ret.push_back(valueObject.getJsonValue().asString());
            }
            tmp = ret;
        }

        template<typename U, typename V>
        void static readJson(JsonObject& msg, const char* key, map<U, V>& tmp)
        {
            /*根据map对象在msg中的key，获取map对象所对应的jsonobject*/
            JsonObject mapObject = msg.getObject(key);
            /*遍历此map的jsonobject*/
            for (Json::Value::const_iterator it = mapObject.begin(); it != mapObject.end(); ++it)
            {
                /*根据此索引的key，从map的jsonobject里读取对应的value*/
                V tv;
                Json::Value vkey = it.key();
                readJson(mapObject, vkey.asString().c_str(), tv);

                /*把json中的key(总是string)转换到真实的key(int或string)*/
                U realKey;
                stringstream ss;
                ss << vkey.asString();
                ss >> realKey;

                /*把value放入到结果map中*/
                tmp[realKey] = tv;
            }
        }

        void static readJson(JsonObject& msg, const char* key, map<string, string>& tmp)
        {
            map<string, string> ret;
            JsonObject mapJson = msg.getObject(key);
            Json::Value::const_iterator itend = mapJson.end();
            for (Json::Value::const_iterator it = mapJson.begin(); it != itend; ++it)
            {
                Json::Value vkey = it.key();
                Json::Value vvalue = *it;
                ret[vkey.asString()] = vvalue.asString();
            }
            tmp = ret;
        }

        void static readJson(JsonObject& msg, const char* key, map<int, string>& tmp)
        {
            map<int, string> ret;
            JsonObject mapJson = msg.getObject(key);
            Json::Value::const_iterator itend = mapJson.end();
            for (Json::Value::const_iterator it = mapJson.begin(); it != itend; ++it)
            {
                Json::Value vkey = it.key();
                Json::Value vvalue = *it;
                ret[atoi(vkey.asString().c_str())] = vvalue.asString();
            }
            tmp = ret;
        }

        void static readJson(JsonObject& msg, const char* key, map<string, int>& tmp)
        {
            map<string, int> ret;
            JsonObject mapJson = msg.getObject(key);
            Json::Value::const_iterator itend = mapJson.end();
            for (Json::Value::const_iterator it = mapJson.begin(); it != itend; ++it)
            {
                Json::Value vkey = it.key();
                Json::Value vvalue = *it;
                ret[vkey.asString()] = vvalue.asInt();
            }
            tmp = ret;
        }

        template<typename T>
        T static readJsonByIndex(JsonObject& msg, int index)
        {
            stringstream ss;
            ss << index;
            T tmp;
            readJson(msg, ss.str().c_str(), tmp);
            return tmp;
        }

    public:
        void    static  writeJson(JsonObject& msg, int value, const char* key)
        {
            msg.setInt(key, value);
        }

        void    static  writeJson(JsonObject& msg, const char* value, const char* key)
        {
            msg.setStr(key, value);
        }

        void    static  writeJson(JsonObject& msg, string value, const char* key)
        {
            msg.setStr(key, value.c_str());
        }

        void    static  writeJson(JsonObject& msg, JsonObject value, const char* key)
        {
            msg.setObject(key, value);
        }

        void    static  writeJson(JsonObject& msg, vector<int> value, const char* key)
        {
            JsonObject arrayObject;
            for (size_t i = 0; i < value.size(); ++i)
            {
                arrayObject.appendInt(value[i]);
            }
            msg.setObject(key, arrayObject);
        }

        void    static  writeJson(JsonObject& msg, vector<string> value, const char* key)
        {
            JsonObject arrayObject;
            for (size_t i = 0; i < value.size(); ++i)
            {
                arrayObject.appendStr(value[i].c_str());
            }
            msg.setObject(key, arrayObject);
        }

        template<typename T, typename V>
        void    static    writeJson(JsonObject& msg, map<T, V> value, const char* key)
        {
            JsonObject mapObject;
            /*遍历此map*/
            for (map<T, V>::iterator it = value.begin(); it != value.end(); ++it)
            {
                stringstream ss;
                ss << it->first;
                /*把value序列化到map的jsonobject中,key就是它在map结构中的key*/
                writeJson(mapObject, it->second, ss.str().c_str());
            }

            /*把此map添加到msg中*/
            msg.setObject(key, mapObject);
        }

        void    static  writeJson(JsonObject& msg, map<string, string> value, const char* key)
        {
            JsonObject mapObject;
            map<string, string>::iterator itend = value.end();
            for (map<string, string>::iterator it = value.begin(); it != itend; ++it)
            {
                mapObject.setStr(it->first.c_str(), it->second.c_str());
            }
            msg.setObject(key, mapObject);
        }

        void    static  writeJson(JsonObject& msg, map<int, string> value, const char* key)
        {
            JsonObject mapObject;
            map<int, string>::iterator itend = value.end();
            for (map<int, string>::iterator it = value.begin(); it != itend; ++it)
            {
                stringstream ss;
                ss << it->first;
                mapObject.setStr(ss.str().c_str(), it->second.c_str());
            }
            msg.setObject(key, mapObject);
        }

        void    static  writeJson(JsonObject& msg, map<string, int> value, const char* key)
        {
            JsonObject mapObject;
            map<string, int>::iterator itend = value.end();
            for (map<string, int>::iterator it = value.begin(); it != itend; ++it)
            {
                mapObject.setInt(it->first.c_str(), it->second);
            }
            msg.setObject(key, mapObject);
        }

        template<typename T>
        void    static  writeJsonByIndex(JsonObject& msg, T t, int index)
        {
            stringstream ss;
            ss << index;
            writeJson(msg, t, ss.str().c_str());
        }
    };

    class LambdaMgr
    {
    public:
        void    callLambda(const char* str)
        {
            JsonObject msgObject;
            msgObject.read(str);

            int id = msgObject.getInt("id");
            JsonObject parmObject = msgObject.getObject("parm");

            assert(mWrapFunctions.find(id) != mWrapFunctions.end());
            if (mWrapFunctions.find(id) != mWrapFunctions.end())
            {
                mWrapFunctions[id](mRealLambdaPtr[id], parmObject.toString().c_str());
            }
        }

        template<typename T>
        void insertLambda(T lambdaObj)
        {
            mReqID++;
            _insertLambda(mReqID, lambdaObj, &T::operator());
        }

        int getCurrentReqID() const
        {
            return mReqID;
        }
    private:
        template<typename RVal, typename T, typename ...Args>
        struct VariadicLambdaFunctor
        {
            VariadicLambdaFunctor(std::function<void(Args...)> f)
            {
                mf = f;
            }

            static void invoke(void* pvoid, const char* str)
            {
                JsonObject msg;
                msg.read(str);
                int parmIndex = 0;
                eval<Args...>(SizeType<sizeof...(Args)>::TYPE(), pvoid, msg, parmIndex);
            }

            template<typename T, typename ...LeftArgs, typename ...NowArgs>
            static  void    eval(int _, void* pvoid, JsonObject& msg, int& parmIndex, const NowArgs&... args)
            {
                T cur_arg = Utils::readJsonByIndex<T>(msg, parmIndex++);
                eval<LeftArgs...>(SizeType<sizeof...(LeftArgs)>::TYPE(), pvoid, msg, parmIndex, args..., cur_arg);
            }

            template<typename ...NowArgs>
            static  void    eval(char _, void* pvoid, JsonObject& msg, int& parmIndex, const NowArgs&... args)
            {
                VariadicLambdaFunctor<RVal, T, Args...>* pthis = (VariadicLambdaFunctor<RVal, T, Args...>*)pvoid;
                (pthis->mf)(args...);
            }
        private:
            std::function<void(Args...)>   mf;
        };

        template<typename LAMBDA_OBJ_TYPE, typename RVal, typename ...Args>
        void _insertLambda(int iid, LAMBDA_OBJ_TYPE obj, RVal(LAMBDA_OBJ_TYPE::*func)(Args...) const)
        {
            void* pbase = new VariadicLambdaFunctor<void, LAMBDA_OBJ_TYPE, Args...>(obj);

            mWrapFunctions[mReqID] = VariadicLambdaFunctor<void, LAMBDA_OBJ_TYPE, Args...>::invoke;
            mRealLambdaPtr[mReqID] = pbase;
        }
    private:
        int     mReqID;

        typedef void(*pf_wrap)(void* pbase, const char* parmStr);
        map<int, pf_wrap>       mWrapFunctions;
        map<int, void*>         mRealLambdaPtr;
    };

    template<bool>
    struct SelectWriteArg;

    template<>
    struct SelectWriteArg<true>
    {
        template<typename ARGTYPE>
        static  void    Write(LambdaMgr& lambdaMgr, JsonObject& parms, ARGTYPE arg, int index)
        {
            lambdaMgr.insertLambda(arg);
        }
    };

    template<>
    struct SelectWriteArg<false>
    {
        template<typename ARGTYPE>
        static  void    Write(LambdaMgr& lambdaMgr, JsonObject& parms, ARGTYPE arg, int index)
        {
            Utils::writeJsonByIndex(parms, arg, index);
        }
    };

    class rpc
    {
    public:
        template<typename F>
        void        def(const char* funname, F func)
        {
            regFunctor(funname, func);
        }

        /*  远程调用，返回值为经过序列化后的消息  */
        template<typename... Args>
        string    call(const char* funname, const Args&... args)
        {
            JsonObject msg;
            msg.setStr("name", funname);

            int old_req_id = mLambdaMgr.getCurrentReqID();
            JsonObject parms;
            int index = 0;
            writeCallArg(parms, index, args...);

            msg.setObject("parm", parms);
            int now_req_id = mLambdaMgr.getCurrentReqID();
            msg.setInt("req_id", old_req_id == now_req_id ? -1 : now_req_id);   /*req_id表示调用方的请求id，服务器(rpc被调用方)通过此id返回消息(返回值)给调用方*/

            return msg.toString();
        }

        /*  被调用方处理收到的rpc消息*/
        void handleRpc(string str)
        {
            JsonObject msg;
            msg.read(str.c_str());

            string funname = msg.getStr("name");
            JsonObject parm = msg.getObject("parm");
            assert(mWrapFunctions.find(funname) != mWrapFunctions.end());
            if (mWrapFunctions.find(funname) != mWrapFunctions.end())
            {
                mWrapFunctions[funname](mRealFunctions[funname], parm.toString().c_str());
            }
        }

        /*  返回数据给RPC调用端    */
        template<typename... Args>
        string    reply(int id, const Args&... args)
        {
            JsonObject msg;
            msg.setInt("id", id);

            JsonObject parms;
            int index = 0;
            writeCallArg(parms, index, args...);

            msg.setObject("parm", parms);

            return msg.toString();
        }

        /*  调用方处理收到的rpc返回值(消息)*/
        void    handleResponse(string str)
        {
            mLambdaMgr.callLambda(str.c_str());
        }

    private:
        void    writeCallArg(JsonObject& msg, int& index){}

        template<typename Arg>
        void    writeCallArg(JsonObject& msg, int& index, const Arg& arg)
        {
            /*只(剩)有一个参数,肯定也为最后一个参数，允许为lambda*/
            _selectWriteArg(msg, arg, index++);
        }

        template<typename Arg1, typename... Args>
        void    writeCallArg(JsonObject& msg, int& index, const Arg1& arg1, const Args&... args)
        {
            Utils::writeJsonByIndex(msg, arg1, index++);
            writeCallArg(msg, index, args...);
        }
    private:

        /*如果是lambda则加入回调管理器，否则添加到rpc参数*/
        template<typename ARGTYPE>
        void    _selectWriteArg(JsonObject& parms, ARGTYPE arg, int index)
        {
            SelectWriteArg<HasCallOperator<ARGTYPE>::value>::Write(mLambdaMgr, parms, arg, index);
        }

    private:
        template<typename RVal, typename ...Args>
        struct VariadicFunctor
        {
            static  void    invoke(void* realfunc, const char* str)
            {
                JsonObject msg;
                msg.read(str);
                int parmIndex = 0;  /*parmIndex作为json中每个变量的key迭代器*/
                eval<Args...>(SizeType<sizeof...(Args)>::TYPE(), realfunc, msg, parmIndex);
            }

            template<typename T, typename ...LeftArgs, typename ...NowArgs>
            static  void    eval(int _, void* realfunc, JsonObject& msg, int& parmIndex, const NowArgs&... args)
            {
                /*args为已经求值的参数列表*/
                /*cur_arg为即将求值的参数*/
                T cur_arg = Utils::readJsonByIndex<T>(msg, parmIndex++);
                eval<LeftArgs...>(SizeType<sizeof...(LeftArgs)>::TYPE(), realfunc, msg, parmIndex, args..., cur_arg);
            }

            template<typename ...NowArgs>
            static  void    eval(char _, void* realfunc, JsonObject& msg, int& parmIndex, const NowArgs&... args)
            {
                /*没有任何剩下的未知参数类型，那么 args 形参就是最终的所有参数，在此回调函数即可*/
                typedef void(*pf)(NowArgs...);
                pf p = (pf)realfunc;
                p(args...);
            }

        private:
        };

        template<typename RVal, typename ...Args>
        void regFunctor(const char* funname, RVal(*func)(Args...))
        {
            mWrapFunctions[funname] = VariadicFunctor<RVal, Args...>::invoke;
            mRealFunctions[funname] = func;
        }
    private:
        typedef void(*pf_wrap)(void* realFunc, const char* parmStr);
        map<string, pf_wrap>    mWrapFunctions; /*  包装函数表   */
        map<string, void*>      mRealFunctions; /*  真实函数表   */
        LambdaMgr               mLambdaMgr;
    };

    template struct SelectWriteArg<true>;
    template struct SelectWriteArg<false>;
}

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

void test4(string a, int b)
{
    cout << "in test4" << endl;
    cout << a << "," << b <<  endl;
}

void test5(string a, int b, map<int, map<int, string>> vlist)
{
}

void test6(string a, int b, map<string, int> vlist)
{
}

void test7()
{
    cout << "in test7" << endl;
}

int main()
{
    int upvalue = 10;
    using namespace dodo;

    rpc rpc_server; /*rpc服务器*/
    rpc rpc_client; /*rpc客户端*/

    rpc_server.def("test4", test4);
    rpc_server.def("test5", test5);
    rpc_server.def("test7", test7);

    string rpc_request_msg; /*  rpc消息   */
    string rpc_response_str;       /*  rpc返回值  */

    {
        rpc_request_msg = rpc_client.call("test7");

        rpc_server.handleRpc(rpc_request_msg);
    }

    map<int, string> m1;
    m1[1] = "Li";
    map<int, string> m2;
    m2[2] = "Deng";
    map<int, map<int, string>> mlist;
    mlist[100] = m1;
    mlist[200] = m2;

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
        rpc_client.handleResponse(rpc_response_str);
    }

    {
        rpc_response_str = rpc_server.reply(2, "hello", "world", 3);
        rpc_client.handleResponse(rpc_response_str);
    }

    cin.get();
    return 0;
}