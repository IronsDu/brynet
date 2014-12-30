#include <stdio.h>
#include <assert.h>
#include <string>
#include <iostream>
#include <sstream>
#include <map>

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

        template<typename T>
        T static readJsonByIndex(JsonObject& msg, int index)
        {
            stringstream ss;
            ss << index;
            return readJson<T>(msg, ss.str().c_str());
        }

    public:
        template<typename T>
        void    static  writeJson(JsonObject& msg, T t, const char* key);

        template<>
        void    static  writeJson<int>(JsonObject& msg, int t, const char* key)
        {
            msg.setInt(key, t);
        }

        template<>
        void    static  writeJson<const char*>(JsonObject& msg, const char* t, const char* key)
        {
            msg.setStr(key, t);
        }

        template<>
        void    static  writeJson<string>(JsonObject& msg, string t, const char* key)
        {
            msg.setStr(key, t.c_str());
        }

        template<typename T>
        void    static  writeJsonByIndex(JsonObject& msg, T t, int index)
        {
            stringstream ss;
            ss << index;
            writeJson<T>(msg, t, ss.str().c_str());
        }
    };

    class rpc
    {
    public:
        template<typename F>
        void        def_fun(const char* funname, F func)
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
            Utils::writeJsonByIndex(parms, p1, index++);

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
            Utils::writeJsonByIndex(parms, p2, index++);
            msg.setObject("parm", parms);

            handleMsg(msg.toString().c_str());
        }

        template<typename PARM1, typename PARM2, typename PARM3>
        void    call(const char* funname, PARM1 p1, PARM2 p2,PARM3 p3)
        {
            JsonObject msg;
            msg.setStr("name", funname);

            JsonObject parms;
            int index = 0;
            Utils::writeJsonByIndex(parms, p1, index++);
            Utils::writeJsonByIndex(parms, p2, index++);
            Utils::writeJsonByIndex(parms, p3, index++);
            msg.setObject("parm", parms);

            handleMsg(msg.toString().c_str());
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

    private:
        typedef void(*pf_wrap)(void* realFunc, const char* parmStr);
        map<string, pf_wrap>    mWrapFunctions; /*  包装函数表   */
        map<string, void*>      mRealFunctions; /*  真实函数表*/
    };
    
}

void test1(int a, int b)
{
    cout << a << ", " << b << endl;
}

void test2(int a, int b, string c)
{
    cout << a << ", " << b << ", " << c << endl;
}

void test3(string a, int b, string c)
{
    cout << a << ", " << b << ", " << c << endl;
}

int main()
{
    using namespace dodo;

    rpc rpc;
    rpc.def_fun("test1", test1);
    rpc.def_fun("test2", test2);
    rpc.def_fun("test3", test3);

    rpc.call("test1", 11, 22);
    rpc.call("test2", 111, 222, "hello");
    rpc.call("test3", "world", 111, "cpp");

    {
        JsonObject msg;
        msg.setStr("name", "test1");
        {
            JsonObject parm;
            parm.setInt("0", 11);
            parm.setInt("1", 22);
            msg.setObject("parm", parm);
        }

        rpc.handleMsg(msg.toString().c_str());
    }

    cin.get();
    return 0;
}