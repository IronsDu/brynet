#ifndef DODO_RPC_COMMON_H_
#define DODO_RPC_COMMON_H_

#include <cassert>
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <tuple>

namespace dodo
{
    namespace rpc
    {
        using namespace std;

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

        template<class Tuple, std::size_t N>
        struct TupleClear
        {
            static void cleanTuple(Tuple& t)
            {
                TupleClear<Tuple, N - 1>::cleanTuple(t);
                clear(std::get<N - 1>(t));
            }
        };

        template<class Tuple>
        struct TupleClear < Tuple, 1 >
        {
            static void cleanTuple(Tuple& t)
            {
                clear(std::get<0>(t));
            }
        };

        template<typename ...Args>
        static void    clear(std::tuple<Args...>& v)
        {
            TupleClear<decltype(v), sizeof...(Args)>::cleanTuple(v);
        }

        template<typename ...Args>
        struct VariadicArgFunctor
        {
            static void  destroy(void* p)
            {
                auto self = static_cast<VariadicArgFunctor<Args...>*>(p);
                delete self;
            }

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
                auto copy = mRealFunctionPtr;
                for (auto& p : copy)
                {
                    del(p.first);
                }
            }

            template<typename T>
            void insertFunction(const string& name, T lambdaObj)
            {
                _insertFunction<T>(name, lambdaObj, &T::operator());
            }

            template<typename ...Args>
            void insertFunction(const string& name, void(*func)(Args...))
            {
                std::function<void(Args...)> f = func;
                _insertFunction<decltype(f)>(name, f, &decltype(f)::operator());
            }

            template<typename FUNTYPE>
            void insertFunction(const string& name, std::function<FUNTYPE> func)
            {
                _insertFunction<decltype(func)>(name, func, &decltype(func)::operator());
            }

            void    del(const string& name)
            {
                auto it = mRealFunctionPtr.find(name);
                auto destroyIT = mDestroyFunctions.find(name);
                if (it != mRealFunctionPtr.end() && destroyIT != mDestroyFunctions.end())
                {
                    (*destroyIT).second((*it).second);
                }

                mWrapFunctions.erase(name);
                mRealFunctionPtr.erase(it);
                mDestroyFunctions.erase(destroyIT);
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
            void _insertFunction(string name, LAMBDA_OBJ_TYPE obj, void(LAMBDA_OBJ_TYPE::*func)(Args...) const)
            {
                void* pbase = new VariadicArgFunctor<Args...>(obj);
                assert(mWrapFunctions.find(name) == mWrapFunctions.end());
                typedef typename INVOKE_TYPE::template Invoke<Args...> TMPTYPE;
                mWrapFunctions[name] = static_cast<CALLBACK_TYPE>(TMPTYPE::invoke);
                mDestroyFunctions[name] = VariadicArgFunctor<Args...>::destroy;
                mRealFunctionPtr[name] = pbase;
            }

        protected:
            typedef void(*FREE_FUNCTION_CALLBACK)(void*);

            map<string, CALLBACK_TYPE>          mWrapFunctions;
            map<string, void*>                  mRealFunctionPtr;
            map<string, FREE_FUNCTION_CALLBACK> mDestroyFunctions;
            int                                 mRequestID;
        };
    }
}

#endif