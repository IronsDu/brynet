// lua_tinker.h
//
// LuaTinker - Simple and light C++ wrapper for Lua.
//
// Copyright (c) 2005-2007 Kwon-il Lee (zupet@hitel.net)
// 
// please check Licence.txt file for licence and legal issues. 

#if !defined(_LUA_TINKER_H_)
#define _LUA_TINKER_H_

#pragma warning (disable:4996)

#include <new>
#include <string>
#include <string.h>

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "luaconf.h"
};

namespace lua_tinker
{
    // init LuaTinker
    void	init(lua_State *L);

    void	init_s64(lua_State *L);
    void	init_u64(lua_State *L);

    // string-buffer excution
    bool	dofile(lua_State *L, const char *filename);
    void	dostring(lua_State *L, const char* buff);
    void	dobuffer(lua_State *L, const char* buff, size_t sz);

    // debug helpers
    void	enum_stack(lua_State *L);
    int		on_error(lua_State *L);
    void	print_error(lua_State *L, const char* fmt, ...);

    // dynamic type extention
    struct lua_value
    {
        virtual void to_lua(lua_State *L) = 0;
    };

    // type trait
    template<typename T> struct class_name;
    struct table;

    template<bool C, typename A, typename B> struct if_ {};
    template<typename A, typename B>		struct if_ < true, A, B > { typedef A type; };
    template<typename A, typename B>		struct if_ < false, A, B > { typedef B type; };

    template<typename A>
    struct is_ptr { static const bool value = false; };
    template<typename A>
    struct is_ptr < A* > { static const bool value = true; };

    template<typename A>
    struct is_ref { static const bool value = false; };
    template<typename A>
    struct is_ref < A& > { static const bool value = true; };

    template<typename A>
    struct remove_const { typedef A type; };
    template<typename A>
    struct remove_const < const A > { typedef A type; };

    template<typename A>
    struct base_type { typedef A type; };
    template<typename A>
    struct base_type < A* > { typedef A type; };
    template<typename A>
    struct base_type < A& > { typedef A type; };

    template<typename A>
    struct class_type { typedef typename remove_const<typename base_type<A>::type>::type type; };

    template<typename A>
    struct is_obj { static const bool value = true; };
    template<> struct is_obj < char > { static const bool value = false; };
    template<> struct is_obj < unsigned char > { static const bool value = false; };
    template<> struct is_obj < short > { static const bool value = false; };
    template<> struct is_obj < unsigned short > { static const bool value = false; };
    template<> struct is_obj < long > { static const bool value = false; };
    template<> struct is_obj < unsigned long > { static const bool value = false; };
    template<> struct is_obj < int > { static const bool value = false; };
    template<> struct is_obj < unsigned int > { static const bool value = false; };
    template<> struct is_obj < float > { static const bool value = false; };
    template<> struct is_obj < double > { static const bool value = false; };
    template<> struct is_obj < char* > { static const bool value = false; };
    template<> struct is_obj < const char* > { static const bool value = false; };
    template<> struct is_obj < bool > { static const bool value = false; };
    template<> struct is_obj < lua_value* > { static const bool value = false; };
    template<> struct is_obj < long long > { static const bool value = false; };
    template<> struct is_obj < unsigned long long > { static const bool value = false; };
    template<> struct is_obj < table > { static const bool value = false; };

    /////////////////////////////////
    enum { no = 1, yes = 2 };
    typedef char(&no_type)[no];
    typedef char(&yes_type)[yes];

    struct int_conv_type { int_conv_type(int); };

    no_type int_conv_tester(...);
    yes_type int_conv_tester(int_conv_type);

    no_type vfnd_ptr_tester(const volatile char *);
    no_type vfnd_ptr_tester(const volatile short *);
    no_type vfnd_ptr_tester(const volatile int *);
    no_type vfnd_ptr_tester(const volatile long *);
    no_type vfnd_ptr_tester(const volatile double *);
    no_type vfnd_ptr_tester(const volatile float *);
    no_type vfnd_ptr_tester(const volatile bool *);
    yes_type vfnd_ptr_tester(const volatile void *);

    template <typename T> T* add_ptr(T&);

    template <bool C> struct bool_to_yesno { typedef no_type type; };
    template <> struct bool_to_yesno < true > { typedef yes_type type; };

    template <typename T>
    struct is_enum
    {
        static T arg;
        static const bool value = ((sizeof(int_conv_tester(arg)) == sizeof(yes_type)) && (sizeof(vfnd_ptr_tester(add_ptr(arg))) == sizeof(yes_type)));
    };
    /////////////////////////////////

    // from lua
    template<typename T>
    struct void2val { static T invoke(void* input){ return *(T*)input; } };
    template<typename T>
    struct void2ptr { static T* invoke(void* input){ return (T*)input; } };
    template<typename T>
    struct void2ref { static T& invoke(void* input){ return *(T*)input; } };

    template<typename T>
    struct void2type
    {
        static T invoke(void* ptr)
        {
            return	if_<is_ptr<T>::value
                , void2ptr<typename base_type<T>::type>
                , typename if_<is_ref<T>::value
                , void2ref<typename base_type<T>::type>
                , void2val<typename base_type<T>::type>
                >::type
            >::type::invoke(ptr);
        }
    };

    struct user
    {
        user(void* p) : m_p(p) {}
        virtual ~user() {}
        void* m_p;
    };

    template<typename T>
    struct user2type { static T invoke(lua_State *L, int index) { return void2type<T>::invoke(lua_touserdata(L, index)); } };

    template<typename T>
    struct lua2enum { static T invoke(lua_State *L, int index) { return (T)(int)lua_tonumber(L, index); } };

    template<typename T>
    struct lua2object
    {
        static T invoke(lua_State *L, int index)
        {
            if (!lua_isuserdata(L, index))
            {
                lua_pushstring(L, "no class at first argument. (forgot ':' expression ?)");
                lua_error(L);
            }
            return void2type<T>::invoke(user2type<user*>::invoke(L, index)->m_p);
        }
    };

    template<typename T>
    T lua2type(lua_State *L, int index)
    {
        return	if_<is_enum<T>::value
            , lua2enum<T>
            , lua2object<T>
        >::type::invoke(L, index);
    }

    template<typename T>
    struct val2user : user
    {
        //val2user() : user(new T) {}
        template<typename ...Args>
        val2user(Args&& ...args) : user(new T(args...)) {}

        ~val2user() { delete ((T*)m_p); }
    };

    template<typename T>
    struct ptr2user : user
    {
        ptr2user(T* t) : user((void*)t) {}
    };

    // to lua
    template<typename T>
    struct val2lua { static void invoke(lua_State *L, T& input){ new(lua_newuserdata(L, sizeof(val2user<T>))) val2user<T>(input); } };
    template<typename T>
    struct ptr2lua { static void invoke(lua_State *L, T* input){ if (input) new(lua_newuserdata(L, sizeof(ptr2user<T>))) ptr2user<T>(input); else lua_pushnil(L); } };
    template<typename T>
    struct ref2lua { static void invoke(lua_State *L, T& input){ new(lua_newuserdata(L, sizeof(ptr2user<T>))) ptr2user<T>(&input); } };

    template<typename T>
    struct enum2lua { static void invoke(lua_State *L, T val) { lua_pushinteger(L, (int)val); } };

    template<typename T>
    struct object2lua
    {
        static void invoke(lua_State *L, T val)
        {
            if_<is_ptr<T>::value
                , ptr2lua<typename base_type<T>::type>
                , typename if_<is_ref<T>::value
                , ref2lua<typename base_type<T>::type>
                , val2lua<typename base_type<T>::type>
                >::type
            >::type::invoke(L, val);

            push_meta(L, class_name<typename class_type<T>::type>::name());
            lua_setmetatable(L, -2);
        }
    };

    template<typename T>
    void type2lua(lua_State *L, T val)
    {
        if_<is_enum<T>::value
            , enum2lua<T>
            , object2lua<T>
        >::type::invoke(L, val);
    }

    // get value from cclosure
    template<typename T>
    T upvalue_(lua_State *L)
    {
        return user2type<T>::invoke(L, lua_upvalueindex(1));
    }

    // read a value from lua stack 
    template<typename T>
    T read(lua_State *L, int index)				{ return lua2type<T>(L, index); }

    template<>	char*				read(lua_State *L, int index);
    template<>	const char*			read(lua_State *L, int index);
    template<>	char				read(lua_State *L, int index);
    template<>	unsigned char		read(lua_State *L, int index);
    template<>	short				read(lua_State *L, int index);
    template<>	unsigned short		read(lua_State *L, int index);
    template<>	long				read(lua_State *L, int index);
    template<>	unsigned long		read(lua_State *L, int index);
    template<>	int					read(lua_State *L, int index);
    template<>	unsigned int		read(lua_State *L, int index);
    template<>	float				read(lua_State *L, int index);
    template<>	double				read(lua_State *L, int index);
    template<>	bool				read(lua_State *L, int index);
    template<>	void				read(lua_State *L, int index);
    template<>	long long			read(lua_State *L, int index);
    template<>	unsigned long long	read(lua_State *L, int index);
    template<>	table				read(lua_State *L, int index);
    template<>	std::string		    read(lua_State *L, int index);
    template<>	lua_State*		    read(lua_State *L, int index);

    // push a value to lua stack 
    template<typename T>
    void push(lua_State *L, T ret)					{ type2lua<T>(L, ret); }

    template<>	void push(lua_State *L, char ret);
    template<>	void push(lua_State *L, unsigned char ret);
    template<>	void push(lua_State *L, short ret);
    template<>	void push(lua_State *L, unsigned short ret);
    template<>	void push(lua_State *L, long ret);
    template<>	void push(lua_State *L, unsigned long ret);
    template<>	void push(lua_State *L, int ret);
    template<>	void push(lua_State *L, unsigned int ret);
    template<>	void push(lua_State *L, float ret);
    template<>	void push(lua_State *L, double ret);
    template<>	void push(lua_State *L, char* ret);
    template<>	void push(lua_State *L, const char* ret);
    template<>  void push(lua_State *L, std::string ret);
    template<>  void push(lua_State *L, const std::string& ret);
    template<>  void push(lua_State *L, std::string&& ret);
    template<>	void push(lua_State *L, bool ret);
    template<>	void push(lua_State *L, lua_value* ret);
    template<>	void push(lua_State *L, long long ret);
    template<>	void push(lua_State *L, unsigned long long ret);
    template<>	void push(lua_State *L, table ret);

    // pop a value from lua stack
    template<typename T>
    T pop(lua_State *L) { T t = read<T>(L, -1); lua_pop(L, 1); return t; }

    template<>	void	pop(lua_State *L);
    template<>	table	pop(lua_State *L);

    static void RecursionRead(lua_State *L, int index)
    {}

    template<typename T0, typename ...TArgs>
    static void RecursionRead(lua_State *L, int index, T0& p0, TArgs& ...args)
    {
        p0 = read<T0>(L, index);
        index++;
        RecursionRead(L, index, args...);
    }

    template<typename RVal, typename ...Args>
    struct HelpEval
    {
        static  void    eval(RVal(*f)(Args...), lua_State *L, Args&&... args)
        {
            RecursionRead(L, 1, args...);
            RVal ret = (f)(args...);
            push<RVal>(L, ret);
        }
    };

    template<typename ...Args>
    struct HelpEval < void, Args... >
    {
        static  void    eval(void(*f)(Args...), lua_State *L, Args&&... args)
        {
            RecursionRead(L, 1, args...);
            (f)(args...);
        }
    };

    // functor (with return value)
    template<typename RVal, typename ...Args>
    struct functor
    {
        static int invoke(lua_State *L)
        {
            auto f = upvalue_<RVal(*)(Args...)>(L);
            HelpEval<RVal, Args...>::eval(f, L, Args()...);
            return 1;
        }
    };

    template<typename ...Args>
    struct functor < void, Args... >
    {
        static int invoke(lua_State *L)
        {
            auto f = upvalue_<void(*)(Args...)>(L);
            HelpEval<void, Args...>::eval(f, L, Args()...);
            return 0;
        }
    };

    // push_functor
    template<typename RVal, typename ...Args>
    void push_functor(lua_State *L, RVal(*func)(Args...))
    {
        lua_pushcclosure(L, functor<RVal, Args...>::invoke, 1);
    }

    // member variable
    struct var_base
    {
        virtual ~var_base(){}
        virtual void get(lua_State *L) = 0;
        virtual void set(lua_State *L) = 0;
    };

    template<typename T, typename V>
    struct mem_var : var_base
    {
        V T::*_var;
        mem_var(V T::*val) : _var(val) {}
        void get(lua_State *L)	{ push<if_<is_obj<V>::value, V&, V>::type>(L, read<T*>(L, 1)->*(_var)); }
        void set(lua_State *L)	{ read<T*>(L, 1)->*(_var) = read<V>(L, 3); }
    };

    template<typename RVal, typename P, typename ...Args>
    struct HelpMemEval
    {
        static  void    eval(P* p, RVal(P::*f)(Args...), lua_State *L, Args&&... args)
        {
            RecursionRead(L, 2, args...);
            RVal ret = (p->*f)(args...);
            push<RVal>(L, ret);
        }
    };

    template<typename P, typename ...Args>
    struct HelpMemEval < void, P, Args... >
    {
        static  void    eval(P* p, void(P::*f)(Args...), lua_State *L, Args&&... args)
        {
            RecursionRead(L, 2, args...);
            (p->*f)(args...);
        }
    };

    // class member functor (with return value)
    template<typename RVal, typename T, typename P, typename ...Args>
    struct mem_functor
    {
        static int invoke(lua_State *L)
        {
            P* p = ((P*)read<T*>(L, 1));
            auto f = upvalue_<RVal(P::*)(Args...)>(L);
            HelpMemEval<RVal, P, Args...>::eval(p, f, L, Args()...);
            return 1;
        }
    };

    template<typename T, typename P, typename ...Args>
    struct mem_functor < void, T, P, Args... >
    {
        static int invoke(lua_State *L)
        {
            P* p = ((P*)read<T*>(L, 1));
            auto f = upvalue_<void(P::*)(Args...)>(L);
            HelpMemEval<void, P, Args...>::eval(p, f, L, Args()...);
            return 0;
        }
    };

    // push_functor
    template<typename T, typename RVal, typename P, typename ...Args>
    void push_mem_functor(lua_State *L, RVal(P::*func)(Args...))
    {
        lua_pushcclosure(L, mem_functor<RVal, T, P, Args...>::invoke, 1);
    }

    template<typename T, typename RVal, typename P, typename ...Args>
    void push_mem_functor(lua_State *L, RVal(P::*func)(Args...) const)
    {
        lua_pushcclosure(L, mem_functor<RVal, T, P, Args...>::invoke, 1);
    }

    template<typename T, typename ...Args>
    struct ConstructorEval
    {
        static  void    eval(void* memory, lua_State *L, Args&&... args)
        {
            RecursionRead(L, 1, args...);
            new(memory)val2user<T>(args...);
        }
    };

    // constructor
    template<typename T, typename ...Args>
    int constructor(lua_State *L)
    {
        void* m = lua_newuserdata(L, sizeof(val2user<T>));
        ConstructorEval<T, Args...>::eval(m, L, Args()...);
        push_meta(L, class_name<typename class_type<T>::type>::name());
        lua_setmetatable(L, -2);

        return 1;
    }

    // destroyer
    template<typename T>
    int destroyer(lua_State *L)
    {
        ((user*)lua_touserdata(L, 1))->~user();
        return 0;
    }

    // global function
    template<typename F>
    void def(lua_State* L, const char* name, F func)
    {
#if(LUA_VERSION_NUM == 501)
        lua_pushstring(L, name);
        lua_pushlightuserdata(L, (void*)func);
        push_functor(L, func);
        lua_settable(L, LUA_GLOBALSINDEX);
#elif(LUA_VERSION_NUM == 503)
        lua_pushlightuserdata(L, (void*)func);
        push_functor(L, func);
        lua_setglobal(L, name);
#endif
    }

    // global variable
    template<typename T>
    void set(lua_State* L, const char* name, T object)
    {
#if(LUA_VERSION_NUM == 501)
        lua_pushstring(L, name);
        push(L, object);
        lua_settable(L, LUA_GLOBALSINDEX);
#elif(LUA_VERSION_NUM == 503)
        push(L, object);
        lua_setglobal(L, name);
#endif
    }

    template<typename T>
    T get(lua_State* L, const char* name)
    {
#if(LUA_VERSION_NUM == 501)
        lua_pushstring(L, name);
        lua_gettable(L, LUA_GLOBALSINDEX);
        return pop<T>(L);
#elif(LUA_VERSION_NUM == 503)
        lua_getglobal(L, name);
        return pop<T>(L);
#endif
    }

    template<typename T>
    void decl(lua_State* L, const char* name, T object)
    {
        set(L, name, object);
    }

    static  void    PushArgs(lua_State *L)
    {
    }

    template<typename T, typename ...LeftArgs>
    static  void    PushArgs(lua_State *L, T&& t, LeftArgs&&... args)
    {
        push(L, t);
        PushArgs(L, args...);
    }

    // call
    template<typename RVal, typename ...Args>
    RVal call(lua_State* L, const char* name, Args&&... args)
    {
        lua_pushcclosure(L, on_error, 0);
        int errfunc = lua_gettop(L);
#if(LUA_VERSION_NUM == 501)
        lua_pushstring(L, name);
        lua_gettable(L, LUA_GLOBALSINDEX);
#elif (LUA_VERSION_NUM == 503)
        lua_getglobal(L, name);
#endif
        if (lua_isfunction(L, -1))
        {
            PushArgs(L, args...);
            lua_pcall(L, sizeof...(Args), 1, errfunc);
        }
        else
        {
            print_error(L, "lua_tinker::call() attempt to call global `%s' (not a function)", name);
        }

        lua_remove(L, errfunc);
        return pop<RVal>(L);
    }

    // class helper
    int meta_get(lua_State *L);
    int meta_set(lua_State *L);
    void push_meta(lua_State *L, const char* name);

    template<typename T>
    static int eq_cppobj(lua_State *L)
    {
        ptr2user<T>* a = (ptr2user<T>*)lua_touserdata(L, 1);
        ptr2user<T>* b = (ptr2user<T>*)lua_touserdata(L, 2);
        if (a != NULL && b != NULL)
        {
            lua_pushboolean(L, a->m_p == b->m_p);
        }
        else
        {
            lua_pushboolean(L, 0);
        }

        return 1;
    }

    // class init
    template<typename T>
    void class_add(lua_State* L, const char* name)
    {
        class_name<T>::name(name);

#if(LUA_VERSION_NUM == 501)
        lua_pushstring(L, name);
        lua_newtable(L);

        lua_pushstring(L, "__name");
        lua_pushstring(L, name);
        lua_rawset(L, -3);

        lua_pushstring(L, "__index");
        lua_pushcclosure(L, meta_get, 0);
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcclosure(L, meta_set, 0);
        lua_rawset(L, -3);

        lua_pushstring(L, "__gc");
        lua_pushcclosure(L, destroyer<T>, 0);
        lua_rawset(L, -3);

        lua_pushstring(L, "__eq");
        lua_pushcclosure(L, eq_cppobj<T>, 0);
        lua_rawset(L, -3);

        lua_settable(L, LUA_GLOBALSINDEX);
#elif(LUA_VERSION_NUM == 503)
        lua_newtable(L);

        lua_pushstring(L, "__name");
        lua_pushstring(L, name);
        lua_rawset(L, -3);

        lua_pushstring(L, "__index");
        lua_pushcclosure(L, meta_get, 0);
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcclosure(L, meta_set, 0);
        lua_rawset(L, -3);

        lua_pushstring(L, "__gc");
        lua_pushcclosure(L, destroyer<T>, 0);
        lua_rawset(L, -3);

        lua_pushstring(L, "__eq");
        lua_pushcclosure(L, eq_cppobj<T>, 0);
        lua_rawset(L, -3);

        lua_setglobal(L, name);
#endif
    }

    // Tinker Class Inheritence
    template<typename T, typename P>
    void class_inh(lua_State* L)
    {
        push_meta(L, class_name<T>::name());
        if (lua_istable(L, -1))
        {
            lua_pushstring(L, "__parent");
            push_meta(L, class_name<P>::name());
            lua_rawset(L, -3);
        }
        lua_pop(L, 1);
    }

    // Tinker Class Constructor
    template<typename T, typename F>
    void class_con(lua_State* L, F func)
    {
        push_meta(L, class_name<T>::name());
        if (lua_istable(L, -1))
        {
            lua_newtable(L);
            lua_pushstring(L, "__call");
            lua_pushcclosure(L, func, 0);
            lua_rawset(L, -3);
            lua_setmetatable(L, -2);
        }
        lua_pop(L, 1);
    }

    // Tinker Class Functions
    template<typename T, typename F>
    void class_def(lua_State* L, const char* name, F func)
    {
        push_meta(L, class_name<T>::name());
        if (lua_istable(L, -1))
        {
            lua_pushstring(L, name);
            new(lua_newuserdata(L, sizeof(F))) F(func);
            push_mem_functor<T>(L, func);
            lua_rawset(L, -3);
        }
        lua_pop(L, 1);
    }

    // Tinker Class Variables
    template<typename T, typename BASE, typename VAR>
    void class_mem(lua_State* L, const char* name, VAR BASE::*val)
    {
        push_meta(L, class_name<T>::name());
        if (lua_istable(L, -1))
        {
            lua_pushstring(L, name);
            new(lua_newuserdata(L, sizeof(mem_var<BASE, VAR>))) mem_var<BASE, VAR>(val);
            lua_rawset(L, -3);
        }
        lua_pop(L, 1);
    }

    template<typename T>
    struct class_name
    {
        // global name
        static const char* name(const char* name = NULL)
        {
            static char temp[256] = "";
            if (name) strcpy(temp, name);
            return temp;
        }
    };

    // Table Object on Stack
    struct table_obj
    {
        table_obj(lua_State* L, int index);
        ~table_obj();

        void inc_ref();
        void dec_ref();

        bool validate();

        template<typename T>
        void set(const char* name, T object)
        {
            if (validate())
            {
                lua_pushstring(m_L, name);
                push(m_L, object);
                lua_settable(m_L, m_index);
            }
        }

        template<typename T>
        T get(const char* name)
        {
            if (validate())
            {
                lua_pushstring(m_L, name);
                lua_gettable(m_L, m_index);
            }
            else
            {
                lua_pushnil(m_L);
            }

            return pop<T>(m_L);
        }

        lua_State*		m_L;
        int				m_index;
        const void*		m_pointer;
        int				m_ref;
    };

    // Table Object Holder
    struct table
    {
        table(lua_State* L);
        table(lua_State* L, int index);
        table(lua_State* L, const char* name);
        table(const table& input);
        ~table();

        template<typename T>
        void set(const char* name, T object)
        {
            m_obj->set(name, object);
        }

        template<typename T>
        T get(const char* name)
        {
            return m_obj->get<T>(name);
        }

        table_obj*		m_obj;
    };

} // namespace lua_tinker

#endif //_LUA_TINKER_H_
