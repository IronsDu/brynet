#include <stdio.h>
#include <assert.h>
#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <functional>

#include "json_object.h"

using namespace std;

namespace dodo
{
    class Utils
    {
    public:
        template<typename T>
        T static readJson(JsonObject& msg, const char* key);

        template<>
        char static readJson<char>(JsonObject& msg, const char* key)
        {
            return msg.getInt(key);
        }

        template<>
        int static readJson<int>(JsonObject& msg, const char* key)
        {
            return msg.getInt(key);
        }

        template<>
        string static readJson<string>(JsonObject& msg, const char* key)
        {
            return msg.getStr(key);
        }

        template<>
        vector<int> static readJson<vector<int>>(JsonObject& msg, const char* key)
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
            return ret;
        }

        template<>
        vector<string> static readJson<vector<string>>(JsonObject& msg, const char* key)
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
            return ret;
        }

        template<>
        map<string, string> static readJson<map<string, string>>(JsonObject& msg, const char* key)
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
            return ret;
        }

        template<>
        map<int, string> static readJson<map<int, string>>(JsonObject& msg, const char* key)
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
            return ret;
        }

        template<>
        map<string, int> static readJson<map<string, int>>(JsonObject& msg, const char* key)
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
            return ret;
        }

        template<typename T>
        T static readJsonByIndex(JsonObject& msg, int index)
        {
            stringstream ss;
            ss << index;
            return readJson<T>(msg, ss.str().c_str());
        }

    public:
        template<typename T>
        void    static  writeJson(JsonObject& msg, T t, const char* key){}

        template<>
        void    static  writeJson<int>(JsonObject& msg, int value, const char* key)
        {
            msg.setInt(key, value);
        }

        template<>
        void    static  writeJson<const char*>(JsonObject& msg, const char* value, const char* key)
        {
            msg.setStr(key, value);
        }

        template<>
        void    static  writeJson<string>(JsonObject& msg, string value, const char* key)
        {
            msg.setStr(key, value.c_str());
        }

        template<>
        void    static  writeJson<vector<int>>(JsonObject& msg, vector<int> value, const char* key)
        {
            JsonObject arrayObject;
            for (size_t i = 0; i < value.size(); ++i)
            {
                arrayObject.appendInt(value[i]);
            }
            msg.setObject(key, arrayObject);
        }

        template<>
        void    static  writeJson<vector<string>>(JsonObject& msg, vector<string> value, const char* key)
        {
            JsonObject arrayObject;
            for (size_t i = 0; i < value.size(); ++i)
            {
                arrayObject.appendStr(value[i].c_str());
            }
            msg.setObject(key, arrayObject);
        }

        template<>
        void    static  writeJson<map<string, string>>(JsonObject& msg, map<string, string> value, const char* key)
        {
            JsonObject mapObject;
            map<string, string>::iterator itend = value.end();
            for (map<string, string>::iterator it = value.begin(); it != itend; ++it)
            {
                mapObject.setStr(it->first.c_str(), it->second.c_str());
            }
            msg.setObject(key, mapObject);
        }

        template<>
        void    static  writeJson<map<int, string>>(JsonObject& msg, map<int, string> value, const char* key)
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

        template<>
        void    static  writeJson<map<string, int>>(JsonObject& msg, map<string, int> value, const char* key)
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
            writeJson<T>(msg, t, ss.str().c_str());
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
            _insertLambda(mNextID, lambdaObj, &T::operator());
            mNextID++;
        }

        template<>
        void insertLambda<int>(int _)
        {
        }
        template<>
        void insertLambda<const char*>(const char* _)
        {
        }
        template<>
        void insertLambda<string>(string _)
        {
        }
        template<>
        void insertLambda<vector<int>>(vector<int> _)
        {
        }
        template<>
        void insertLambda<vector<string>>(vector<string> _)
        {
        }
        template<>
        void insertLambda<map<string, string>>(map<string, string> _)
        {
        }
        template<>
        void insertLambda<map<int, string>>(map<int, string> _)
        {
        }
        template<>
        void insertLambda<map<string, int>>(map<string, int> _)
        {
        }
    private:
        template<typename RVal, typename T, typename T1 = void, typename T2 = void, typename T3 = void, typename T4 = void, typename T5 = void, typename T6 = void, typename T7 = void>
        struct LambdaFunctor
        {
            static int invoke(void* pvoid, const char* str)
            {
            }
        };

        template<typename RVal, typename T, typename T1, typename T2, typename T3, typename T4>
        struct LambdaFunctor<RVal, T, T1, T2, T3, T4>
        {
        public:
            LambdaFunctor(std::function<void(T1, T2, T3, T4)> f)
            {
                mf = f;
            }

            static void invoke(void* pvoid, const char* str)
            {
                JsonObject jsonObject;
                jsonObject.read(str);

                int parmIndex = 0;
                T1 parm1 = Utils::readJsonByIndex<T1>(jsonObject, parmIndex++);
                T2 parm2 = Utils::readJsonByIndex<T2>(jsonObject, parmIndex++);
                T3 parm3 = Utils::readJsonByIndex<T3>(jsonObject, parmIndex++);
                T4 parm4 = Utils::readJsonByIndex<T4>(jsonObject, parmIndex++);

                LambdaFunctor<RVal, T, T1, T2, T3, T4>* pthis = (LambdaFunctor<RVal, T, T1, T2, T3, T4>*)pvoid;
                (pthis->mf)(parm1, parm2, parm3, parm4);
            }

        private:
            std::function<void(T1, T2, T3, T4)>   mf;
        };

        template<typename RVal, typename T, typename T1, typename T2, typename T3>
        struct LambdaFunctor<RVal, T, T1, T2, T3>
        {
        public:
            LambdaFunctor(std::function<void(T1, T2, T3)> f)
            {
                mf = f;
            }

            static void invoke(void* pvoid, const char* str)
            {
                JsonObject jsonObject;
                jsonObject.read(str);

                int parmIndex = 0;
                T1 parm1 = Utils::readJsonByIndex<T1>(jsonObject, parmIndex++);
                T2 parm2 = Utils::readJsonByIndex<T2>(jsonObject, parmIndex++);
                T3 parm3 = Utils::readJsonByIndex<T3>(jsonObject, parmIndex++);

                LambdaFunctor<RVal, T, T1, T2, T3>* pthis = (LambdaFunctor<RVal, T, T1, T2, T3>*)pvoid;
                (pthis->mf)(parm1, parm2, parm3);
            }

        private:
            std::function<void(T1, T2, T3)>   mf;
        };

        template<typename RVal, typename T, typename T1, typename T2>
        struct LambdaFunctor<RVal, T, T1, T2>
        {
        public:
            LambdaFunctor(std::function<void(T1, T2)> f)
            {
                mf = f;
            }

            static void invoke(void* pvoid, const char* str)
            {
                JsonObject jsonObject;
                jsonObject.read(str);

                int parmIndex = 0;
                T1 parm1 = Utils::readJsonByIndex<T1>(jsonObject, parmIndex++);
                T2 parm2 = Utils::readJsonByIndex<T2>(jsonObject, parmIndex++);

                LambdaFunctor<RVal, T, T1, T2>* pthis = (LambdaFunctor<RVal, T, T1, T2>*)pvoid;
                (pthis->mf)(parm1, parm2);
            }

        private:
            std::function<void(T1, T2)>   mf;
        };

        template<typename RVal, typename T, typename T1>
        struct LambdaFunctor<RVal, T, T1>
        {
        public:
            LambdaFunctor(std::function<void(T1)> f)
            {
                mf = f;
            }

            static void invoke(void* pvoid, const char* str)
            {
                JsonObject jsonObject;
                jsonObject.read(str);

                int parmIndex = 0;
                T1 parm1 = Utils::readJsonByIndex<T1>(jsonObject, parmIndex++);

                LambdaFunctor<RVal, T, T1>* pthis = (LambdaFunctor<RVal, T, T1>*)pvoid;
                (pthis->mf)(parm1, parm2);
            }
        private:
            std::function<void(T1)>   mf;
        };

        template<typename RVal, typename T>
        struct LambdaFunctor<RVal, T>
        {
        public:
            LambdaFunctor(std::function<void(void)> f)
            {
                mf = f;
            }

            static void invoke(void* pvoid, const char* str)
            {
                LambdaFunctor<RVal, T>* pthis = (LambdaFunctor<RVal, T>*)pvoid;
                (pthis->mf)();
            }

        private:
            std::function<void(void)>   mf;
        };

        template<typename LAMBDA_OBJ_TYPE, typename RVal>
        void _insertLambda(int iid, LAMBDA_OBJ_TYPE obj, RVal(LAMBDA_OBJ_TYPE::*func)() const)
        {
            void* pbase = new LambdaFunctor<void, LAMBDA_OBJ_TYPE>(obj);

            mWrapFunctions[mNextID] = LambdaFunctor<void, LAMBDA_OBJ_TYPE>::invoke;
            mRealLambdaPtr[mNextID] = pbase;
        }


        template<typename LAMBDA_OBJ_TYPE, typename RVal, typename T1>
        void _insertLambda(int iid, LAMBDA_OBJ_TYPE obj, RVal(LAMBDA_OBJ_TYPE::*func)(T1) const)
        {
            void* pbase = new LambdaFunctor<void, LAMBDA_OBJ_TYPE, T1>(obj);

            mWrapFunctions[mNextID] = LambdaFunctor<void, LAMBDA_OBJ_TYPE, T1>::invoke;
            mRealLambdaPtr[mNextID] = pbase;
        }

        template<typename LAMBDA_OBJ_TYPE, typename RVal, typename T1, typename T2>
        void _insertLambda(int iid, LAMBDA_OBJ_TYPE obj, RVal(LAMBDA_OBJ_TYPE::*func)(T1, T2) const)
        {
            void* pbase = new LambdaFunctor<void, LAMBDA_OBJ_TYPE, T1, T2>(obj);

            mWrapFunctions[mNextID] = LambdaFunctor<void, LAMBDA_OBJ_TYPE, T1, T2>::invoke;
            mRealLambdaPtr[mNextID] = pbase;
        }

        template<typename LAMBDA_OBJ_TYPE, typename RVal, typename T1, typename T2, typename T3>
        void _insertLambda(int iid, LAMBDA_OBJ_TYPE obj, RVal(LAMBDA_OBJ_TYPE::*func)(T1, T2, T3) const)
        {
            void* pbase = new LambdaFunctor<void, LAMBDA_OBJ_TYPE, T1, T2, T3>(obj);

            mWrapFunctions[mNextID] = LambdaFunctor<void, LAMBDA_OBJ_TYPE, T1, T2, T3>::invoke;
            mRealLambdaPtr[mNextID] = pbase;
        }

        template<typename LAMBDA_OBJ_TYPE, typename RVal, typename T1, typename T2, typename T3, typename T4>
        void _insertLambda(int iid, LAMBDA_OBJ_TYPE obj, RVal(LAMBDA_OBJ_TYPE::*func)(T1, T2, T3, T4) const)
        {
            void* pbase = new LambdaFunctor<void, LAMBDA_OBJ_TYPE, T1, T2, T3, T4>(obj);

            mWrapFunctions[mNextID] = LambdaFunctor<void, LAMBDA_OBJ_TYPE, T1, T2, T3, T4>::invoke;
            mRealLambdaPtr[mNextID] = pbase;
        }
    private:
        int     mNextID;

        typedef void(*pf_wrap)(void* pbase, const char* parmStr);
        map<int, pf_wrap>       mWrapFunctions;
        map<int, void*>         mRealLambdaPtr;
    };

    class rpc
    {
    public:
        template<typename F>
        void        def(const char* funname, F func)
        {
            regFunctor(funname, func);
        }

        template<typename PARM1>
        void    call(const char* funname, PARM1 p1)
        {
            JsonObject msg;
            msg.setStr("name", funname);
            JsonObject parms;
            int index = 0;
            _selectWriteArg(parms, p1, index++);

            handleMsg(msg.toString().c_str());
        }

        template<typename PARM1, typename PARM2>
        void    call(const char* funname, PARM1 p1, PARM2 p2)
        {
            JsonObject msg;
            msg.setStr("name", funname);

            JsonObject parms;
            int index = 0;
            Utils::writeJsonByIndex(parms, p1, index++);
            _selectWriteArg(parms, p2, index++);

            msg.setObject("parm", parms);

            handleMsg(msg.toString().c_str());
        }

        template<typename PARM1, typename PARM2, typename PARM3>
        void    call(const char* funname, PARM1 p1, PARM2 p2, PARM3 p3)
        {
            JsonObject msg;
            msg.setStr("name", funname);

            JsonObject parms;
            int index = 0;
            Utils::writeJsonByIndex(parms, p1, index++);
            Utils::writeJsonByIndex(parms, p2, index++);
            _selectWriteArg(parms, p3, index++);

            msg.setObject("parm", parms);

            handleMsg(msg.toString().c_str());
        }

        template<typename PARM1, typename PARM2, typename PARM3, typename PARM4>
        void    call(const char* funname, PARM1 p1, PARM2 p2, PARM3 p3, PARM4 p4)
        {
            JsonObject msg;
            msg.setStr("name", funname);

            JsonObject parms;
            int index = 0;
            Utils::writeJsonByIndex(parms, p1, index++);
            Utils::writeJsonByIndex(parms, p2, index++);
            Utils::writeJsonByIndex(parms, p3, index++);
            _selectWriteArg(parms, p4, index++);

            msg.setObject("parm", parms);

            handleMsg(msg.toString().c_str());
        }

        void    callLambda(const char* str)
        {
            mLambdaMgr.callLambda(str);
        }

        void handleMsg(const char* str)
        {
            JsonObject msg;
            msg.read(str);

            string funname = msg.getStr("name");
            JsonObject parm = msg.getObject("parm");
            assert(mWrapFunctions.find(funname) != mWrapFunctions.end());
            if (mWrapFunctions.find(funname) != mWrapFunctions.end())
            {
                mWrapFunctions[funname](mRealFunctions[funname], parm.toString().c_str());
            }
        }

    private:
        template<typename TESTTYPE>
        struct TypeIsLambda
        {
            template<typename TESTTYPE>
            struct _Select
            {
                enum
                {
                    value = true
                };
            };

            template<>
            struct _Select<int>
            {
                enum
                {
                    value = false
                };
            };
            template<>
            struct _Select<const char*>
            {
                enum
                {
                    value = false
                };
            };
            template<>
            struct _Select<string>
            {
                enum
                {
                    value = false
                };
            };
            template<>
            struct _Select<vector<int>>
            {
                enum
                {
                    value = false
                };
            };
            template<>
            struct _Select<vector<string>>
            {
                enum
                {
                    value = false
                };
            };
            template<>
            struct _Select<map<string, string>>
            {
                enum
                {
                    value = false
                };
            };
            template<>
            struct _Select<map<int, string>>
            {
                enum
                {
                    value = false
                };
            };
            template<>
            struct _Select<map<string, int>>
            {
                enum
                {
                    value = false
                };
            };

            enum
            {
                Result = _Select<TESTTYPE>::value
            };
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

        /*如果是lambda则加入回调管理器，否则添加到rpc参数*/
        template<typename ARGTYPE>
        void    _selectWriteArg(JsonObject& parms, ARGTYPE arg, int index)
        {
            {
                static SelectWriteArg<true> _tempT;
                static SelectWriteArg<false> _tempF;
            }

            SelectWriteArg<TypeIsLambda<ARGTYPE>::Result>::Write(mLambdaMgr, parms, arg, index);
        }

    private:
        template<typename RVal, typename PARM1 = void, typename PARM2 = void, typename PARM3 = void, typename T4 = void, typename T5 = void, typename T6 = void, typename T7 = void>
        struct functor
        {
            static void invoke(void* realfunc, const char* str);
        };

        template<typename RVal>
        struct functor<RVal>
        {
            static void invoke(void* realfunc, const char* str)
            {
                typedef void(*pf_0parm)();
                pf_0parm p = (pf_0parm)realfunc;
                p();
            }
        };

        template<typename RVal, typename PARM1>
        struct functor<RVal, PARM1>
        {
            static void invoke(void* realfunc, const char* str)
            {
                JsonObject jsonObject;
                jsonObject.read(str);

                int parmIndex = 0;
                PARM1 parm1 = Utils::readJsonByIndex<PARM1>(jsonObject, parmIndex++);

                typedef void(*pf_1parm)(PARM1);
                pf_1parm p = (pf_1parm)realfunc;
                p(parm1);
            }
        };

        template<typename RVal, typename PARM1, typename PARM2>
        struct functor<RVal, PARM1, PARM2>
        {
            static void invoke(void* realfunc, const char* str)
            {
                JsonObject jsonObject;
                jsonObject.read(str);

                int parmIndex = 0;
                PARM1 parm1 = Utils::readJsonByIndex<PARM1>(jsonObject, parmIndex++);
                PARM2 parm2 = Utils::readJsonByIndex<PARM2>(jsonObject, parmIndex++);

                typedef void(*pf_2parm)(PARM1, PARM2);
                pf_2parm p = (pf_2parm)realfunc;
                p(parm1, parm2);
            }
        };

        template<typename RVal, typename PARM1, typename PARM2, typename PARM3>
        struct functor<RVal, PARM1, PARM2, PARM3>
        {
            static void invoke(void* realfunc, const char* str)
            {
                JsonObject jsonObject;
                jsonObject.read(str);

                int parmIndex = 0;
                PARM1 parm1 = Utils::readJsonByIndex<PARM1>(jsonObject, parmIndex++);
                PARM2 parm2 = Utils::readJsonByIndex<PARM2>(jsonObject, parmIndex++);
                PARM3 parm3 = Utils::readJsonByIndex<PARM3>(jsonObject, parmIndex++);

                typedef void(*pf_3parm)(PARM1, PARM2, PARM3);
                pf_3parm p = (pf_3parm)realfunc;
                p(parm1, parm2, parm3);
            }
        };

        template<typename RVal, typename PARM1, typename PARM2, typename PARM3, typename PARM4>
        struct functor<RVal, PARM1, PARM2, PARM3, PARM4>
        {
            static void invoke(void* realfunc, const char* str)
            {
                JsonObject jsonObject;
                jsonObject.read(str);

                int parmIndex = 0;
                PARM1 parm1 = Utils::readJsonByIndex<PARM1>(jsonObject, parmIndex++);
                PARM2 parm2 = Utils::readJsonByIndex<PARM2>(jsonObject, parmIndex++);
                PARM3 parm3 = Utils::readJsonByIndex<PARM3>(jsonObject, parmIndex++);
                PARM4 parm4 = Utils::readJsonByIndex<PARM4>(jsonObject, parmIndex++);

                typedef void(*pf_4parm)(PARM1, PARM2, PARM3, PARM4);
                pf_4parm p = (pf_4parm)realfunc;
                p(parm1, parm2, parm3, parm4);
            }
        };

        template<typename RVal>
        void regFunctor(const char* funname, RVal(*func)())
        {
            mWrapFunctions[funname] = functor<RVal>::invoke;
            mRealFunctions[funname] = func;
        }

        template<typename RVal, typename PARM1>
        void regFunctor(const char* funname, RVal(*func)(PARM1))
        {
            mWrapFunctions[funname] = functor<RVal, PARM1>::invoke;
            mRealFunctions[funname] = func;
        }

        template<typename RVal, typename PARM1, typename PARM2>
        void regFunctor(const char* funname, RVal(*func)(PARM1, PARM2))
        {
            mWrapFunctions[funname] = functor<RVal, PARM1, PARM2>::invoke;
            mRealFunctions[funname] = func;
        }

        template<typename RVal, typename PARM1, typename PARM2, typename PARM3>
        void regFunctor(const char* funname, RVal(*func)(PARM1, PARM2, PARM3))
        {
            mWrapFunctions[funname] = functor<RVal, PARM1, PARM2, PARM3>::invoke;
            mRealFunctions[funname] = func;
        }

        template<typename RVal, typename PARM1, typename PARM2, typename PARM3, typename PARM4>
        void regFunctor(const char* funname, RVal(*func)(PARM1, PARM2, PARM3, PARM4))
        {
            mWrapFunctions[funname] = functor<RVal, PARM1, PARM2, PARM3, PARM4>::invoke;
            mRealFunctions[funname] = func;
        }

    private:
        typedef void(*pf_wrap)(void* realFunc, const char* parmStr);
        map<string, pf_wrap>    mWrapFunctions; /*  包装函数表   */
        map<string, void*>      mRealFunctions; /*  真实函数表   */
        LambdaMgr               mLambdaMgr;
    };

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

void test4(string a, int b, string c)
{
    cout << "in test4" << endl;
    cout << a << "," << b << "," << c << endl;
}

void test5(string a, int b, string c, map<string, string> vlist)
{
}

void test6(string a, int b, string c, map<string, int> vlist)
{
}

int main()
{
    int upvalue = 10;
    using namespace dodo;
    rpc rpc;
    rpc.def("test4", test4);

    map<int, string> vlist = { { 1, "Li" }, { 2, "Deng" } };

    /*调用远程函数,并设置lambda回调函数*/
    rpc.call("test4", "a", 1, "b", [&upvalue](int a, int b){
        upvalue++;
        cout << "upvalue:" << upvalue << ", a:" << a << ", b:" << b << endl;
    });
    rpc.call("test4", "a", 1, "b", [&upvalue](string a, string b, int c){
        upvalue++;
        cout << "upvalue:" << upvalue << ", a:" << a << ", b:" << b << ", c:" << c << endl;
    });
    /*无lambda回调*/
    rpc.call("test4", "a", 1, "b");

    /*模拟(被调用方)触发调用方的lambda函数*/
    {
        JsonObject msg;
        msg.setInt("id", 0);        /*设置远程调用的lambda id*/

        {
            JsonObject parm;        /*添加远程调用的参数*/
            parm.setInt("0", 1);
            parm.setInt("1", 2);
            msg.setObject("parm", parm);
        }

        /*模拟接收到调用方的reply消息，触发本方的lambda函数*/
        rpc.callLambda(msg.toString().c_str());
    }

    {
        JsonObject msg;
        msg.setInt("id", 1);

        {
            JsonObject parm;
            parm.setStr("0", "hello");
            parm.setStr("1", "world");
            parm.setInt("2", 3);
            msg.setObject("parm", parm);
        }
        rpc.callLambda(msg.toString().c_str());
    }

    cin.get();
    return 0;
}