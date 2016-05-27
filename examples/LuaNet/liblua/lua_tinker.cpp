// lua_tinker.cpp
//
// LuaTinker - Simple and light C++ wrapper for Lua.
//
// Copyright (c) 2005-2007 Kwon-il Lee (zupet@hitel.net)
// 
// please check Licence.txt file for licence and legal issues. 

#include <iostream>
#include <string>

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
};

#include "lua_tinker.h"


/*---------------------------------------------------------------------------*/
/* init                                                                      */
/*---------------------------------------------------------------------------*/
void lua_tinker::init(lua_State *L)
{
    init_s64(L);
    init_u64(L);
}

/*---------------------------------------------------------------------------*/
/* __s64                                                                     */
/*---------------------------------------------------------------------------*/
static int tostring_s64(lua_State *L)
{
    char temp[64];
    sprintf(temp, "%lld", *(long long*)lua_topointer(L, 1));
    lua_pushstring(L, temp);
    return 1;
}

/*---------------------------------------------------------------------------*/
static int eq_s64(lua_State *L)
{
    lua_pushboolean(L, memcmp(lua_topointer(L, 1), lua_topointer(L, 2), sizeof(long long)) == 0);
    return 1;
}

/*---------------------------------------------------------------------------*/
static int lt_s64(lua_State *L)
{
    lua_pushboolean(L, memcmp(lua_topointer(L, 1), lua_topointer(L, 2), sizeof(long long)) < 0);
    return 1;
}

/*---------------------------------------------------------------------------*/
static int le_s64(lua_State *L)
{
    lua_pushboolean(L, memcmp(lua_topointer(L, 1), lua_topointer(L, 2), sizeof(long long)) <= 0);
    return 1;
}

/*---------------------------------------------------------------------------*/
void lua_tinker::init_s64(lua_State *L)
{
#if(LUA_VERSION_NUM == 501)
    const char* name = "__s64";
    lua_pushstring(L, name);
    lua_newtable(L);

    lua_pushstring(L, "__name");
    lua_pushstring(L, name);
    lua_rawset(L, -3);

    lua_pushstring(L, "__tostring");
    lua_pushcclosure(L, tostring_s64, 0);
    lua_rawset(L, -3);

    lua_pushstring(L, "__eq");
    lua_pushcclosure(L, eq_s64, 0);
    lua_rawset(L, -3);

    lua_pushstring(L, "__lt");
    lua_pushcclosure(L, lt_s64, 0);
    lua_rawset(L, -3);

    lua_pushstring(L, "__le");
    lua_pushcclosure(L, le_s64, 0);
    lua_rawset(L, -3);

    lua_settable(L, LUA_GLOBALSINDEX);
#endif
}

/*---------------------------------------------------------------------------*/
/* __u64                                                                     */
/*---------------------------------------------------------------------------*/
static int tostring_u64(lua_State *L)
{
    char temp[64];
    sprintf(temp, "%lld", *(unsigned long long*)lua_topointer(L, 1));
    lua_pushstring(L, temp);
    return 1;
}

/*---------------------------------------------------------------------------*/
static int eq_u64(lua_State *L)
{
    lua_pushboolean(L, memcmp(lua_topointer(L, 1), lua_topointer(L, 2), sizeof(unsigned long long)) == 0);
    return 1;
}

/*---------------------------------------------------------------------------*/
static int lt_u64(lua_State *L)
{
    lua_pushboolean(L, memcmp(lua_topointer(L, 1), lua_topointer(L, 2), sizeof(unsigned long long)) < 0);
    return 1;
}

/*---------------------------------------------------------------------------*/
static int le_u64(lua_State *L)
{
    lua_pushboolean(L, memcmp(lua_topointer(L, 1), lua_topointer(L, 2), sizeof(unsigned long long)) <= 0);
    return 1;
}

/*---------------------------------------------------------------------------*/
void lua_tinker::init_u64(lua_State *L)
{
#if(LUA_VERSION_NUM == 501)
    const char* name = "__u64";
    lua_pushstring(L, name);
    lua_newtable(L);

    lua_pushstring(L, "__name");
    lua_pushstring(L, name);
    lua_rawset(L, -3);

    lua_pushstring(L, "__tostring");
    lua_pushcclosure(L, tostring_u64, 0);
    lua_rawset(L, -3);

    lua_pushstring(L, "__eq");
    lua_pushcclosure(L, eq_u64, 0);
    lua_rawset(L, -3);

    lua_pushstring(L, "__lt");
    lua_pushcclosure(L, lt_u64, 0);
    lua_rawset(L, -3);

    lua_pushstring(L, "__le");
    lua_pushcclosure(L, le_u64, 0);
    lua_rawset(L, -3);

    lua_settable(L, LUA_GLOBALSINDEX);
#endif
}

/*---------------------------------------------------------------------------*/
/* excution                                                                  */
/*---------------------------------------------------------------------------*/
bool lua_tinker::dofile(lua_State *L, const char *filename)
{
    bool ret = true;
    lua_pushcclosure(L, on_error, 0);
    int errfunc = lua_gettop(L);

    if (luaL_loadfile(L, filename) == 0)
    {
        lua_pcall(L, 0, 1, errfunc);
    }
    else
    {
        print_error(L, "%s", lua_tostring(L, -1));
        ret = false;
    }

    lua_remove(L, errfunc);
    lua_pop(L, 1);

    return ret;
}

/*---------------------------------------------------------------------------*/
void lua_tinker::dostring(lua_State *L, const char* buff)
{
    lua_tinker::dobuffer(L, buff, strlen(buff));
}

/*---------------------------------------------------------------------------*/
void lua_tinker::dobuffer(lua_State *L, const char* buff, size_t len)
{
    lua_pushcclosure(L, on_error, 0);
    int errfunc = lua_gettop(L);

    if (luaL_loadbuffer(L, buff, len, "lua_tinker::dobuffer()") == 0)
    {
        lua_pcall(L, 0, 1, errfunc);
    }
    else
    {
        print_error(L, "%s", lua_tostring(L, -1));
    }

    lua_remove(L, errfunc);
    lua_pop(L, 1);
}

/*---------------------------------------------------------------------------*/
/* debug helpers                                                             */
/*---------------------------------------------------------------------------*/

static void call_stack(lua_State* L, int n, std::string& temp)
{
    lua_Debug ar;
    if (lua_getstack(L, n, &ar) == 1)
    {
        lua_getinfo(L, "nSlu", &ar);
        char txt[4096];

        const char* indent;
        if (n == 0)
        {
            indent = "->\t";
            sprintf(txt, "%s", "\t<call stack> \n");
            temp += txt;
        }
        else
        {
            indent = "\t";
        }

        if (ar.name)
        {
            sprintf(txt, "%s%s() : line %d [%s : line %d] \n", indent, ar.name, ar.currentline, ar.source, ar.linedefined);
        }
        else
        {
            sprintf(txt, "%sunknown : line %d [%s : line %d] \n", indent, ar.currentline, ar.source, ar.linedefined);
        }

        temp += txt;
        call_stack(L, n + 1, temp);
    }
}

/*---------------------------------------------------------------------------*/
int lua_tinker::on_error(lua_State *L)
{
    std::string  temp;
    temp += lua_tostring(L, -1);
    temp += "\n";
    call_stack(L, 0, temp);

    lua_tinker::print_error(L, "%s", temp.c_str());

    return 0;
}

/*---------------------------------------------------------------------------*/
void lua_tinker::print_error(lua_State *L, const char* fmt, ...)
{
    char text[4096];

    va_list args;
    va_start(args, fmt);
    vsprintf(text, fmt, args);
    va_end(args);
#if(LUA_VERSION_NUM == 501)
    lua_pushstring(L, "_ALERT");
    lua_gettable(L, LUA_GLOBALSINDEX);
#elif(LUA_VERSION_NUM == 503)
    lua_getglobal(L, "_ALERT");
#endif
    
    if (lua_isfunction(L, -1))
    {
        lua_pushstring(L, text);
        lua_call(L, 1, 0);
    }
    else
    {
        printf("%s\n", text);
        lua_pop(L, 1);
    }
}

/*---------------------------------------------------------------------------*/
void lua_tinker::enum_stack(lua_State *L)
{
    int top = lua_gettop(L);
    print_error(L, "Type:%d", top);
    for (int i = 1; i <= lua_gettop(L); ++i)
    {
        switch (lua_type(L, i))
        {
            case LUA_TNIL:
                print_error(L, "\t%s", lua_typename(L, lua_type(L, i)));
                break;
            case LUA_TBOOLEAN:
                print_error(L, "\t%s	%s", lua_typename(L, lua_type(L, i)), lua_toboolean(L, i) ? "true" : "false");
                break;
            case LUA_TLIGHTUSERDATA:
                print_error(L, "\t%s	0x%08p", lua_typename(L, lua_type(L, i)), lua_topointer(L, i));
                break;
            case LUA_TNUMBER:
                print_error(L, "\t%s	%f", lua_typename(L, lua_type(L, i)), lua_tonumber(L, i));
                break;
            case LUA_TSTRING:
                print_error(L, "\t%s	%s", lua_typename(L, lua_type(L, i)), lua_tostring(L, i));
                break;
            case LUA_TTABLE:
                print_error(L, "\t%s	0x%08p", lua_typename(L, lua_type(L, i)), lua_topointer(L, i));
                break;
            case LUA_TFUNCTION:
                print_error(L, "\t%s()	0x%08p", lua_typename(L, lua_type(L, i)), lua_topointer(L, i));
                break;
            case LUA_TUSERDATA:
                print_error(L, "\t%s	0x%08p", lua_typename(L, lua_type(L, i)), lua_topointer(L, i));
                break;
            case LUA_TTHREAD:
                print_error(L, "\t%s", lua_typename(L, lua_type(L, i)));
                break;
        }
    }
}

/*---------------------------------------------------------------------------*/
/* read                                                                      */
/*---------------------------------------------------------------------------*/
template<>
char* lua_tinker::read(lua_State *L, int index)
{
    return (char*)lua_tostring(L, index);
}

template<>
const char* lua_tinker::read(lua_State *L, int index)
{
    return (const char*)lua_tostring(L, index);
}

template<>
std::string lua_tinker::read(lua_State *L, int index)
{
    size_t len = 0;
    const char* b = lua_tolstring(L, index, &len);
    if (b != nullptr)
    {
        return std::string(b, len);
    }
    else
    {
        return "";
    }
}

template<>
char lua_tinker::read(lua_State *L, int index)
{
    return (char)lua_tointeger(L, index);
}

template<>
unsigned char lua_tinker::read(lua_State *L, int index)
{
    return (unsigned char)lua_tointeger(L, index);
}

template<>
short lua_tinker::read(lua_State *L, int index)
{
    return (short)lua_tointeger(L, index);
}

template<>
unsigned short lua_tinker::read(lua_State *L, int index)
{
    return (unsigned short)lua_tointeger(L, index);
}

template<>
long lua_tinker::read(lua_State *L, int index)
{
    return (long)lua_tointeger(L, index);
}

template<>
unsigned long lua_tinker::read(lua_State *L, int index)
{
    return (unsigned long)lua_tointeger(L, index);
}

template<>
int lua_tinker::read(lua_State *L, int index)
{
    return (int)lua_tointeger(L, index);
}

template<>
unsigned int lua_tinker::read(lua_State *L, int index)
{
    return (unsigned int)lua_tointeger(L, index);
}

template<>
float lua_tinker::read(lua_State *L, int index)
{
    return (float)lua_tonumber(L, index);
}

template<>
double lua_tinker::read(lua_State *L, int index)
{
    return (double)lua_tonumber(L, index);
}

template<>
bool lua_tinker::read(lua_State *L, int index)
{
    if (lua_isboolean(L, index))
        return lua_toboolean(L, index) != 0;
    else if (lua_isinteger(L, index))
        return lua_tointeger(L, index) != 0;
    else if (lua_isnumber(L, index))
        return lua_tonumber(L, index) != 0;
    else
        return false;
}

template<>
void lua_tinker::read(lua_State *L, int index)
{
    return;
}

template<>
long long lua_tinker::read(lua_State *L, int index)
{
    if (lua_isinteger(L, index))
        return (long long)lua_tointeger(L, index);
    else if (lua_isnumber(L, index))
        return (long long)lua_tonumber(L, index);
    else if (lua_isuserdata(L, index))
        return *(long long*)lua_touserdata(L, index);
    else
        return 0;
}
template<>
unsigned long long lua_tinker::read(lua_State *L, int index)
{
    if (lua_isinteger(L, index))
        return (unsigned long long)lua_tointeger(L, index);
    else if (lua_isnumber(L, index))
        return (unsigned long long)lua_tonumber(L, index);
    else if (lua_isuserdata(L, index))
        return *(unsigned long long*)lua_touserdata(L, index);
    else
        return 0;
}

template<>
lua_tinker::table lua_tinker::read(lua_State *L, int index)
{
    return table(L, index);
}

template<>
lua_State* lua_tinker::read(lua_State *L, int index)
{
    return lua_tothread(L, index);
}

/*---------------------------------------------------------------------------*/
/* push                                                                      */
/*---------------------------------------------------------------------------*/
template<>
void lua_tinker::push(lua_State *L, char ret)
{
    lua_pushinteger(L, ret);
}

template<>
void lua_tinker::push(lua_State *L, unsigned char ret)
{
    lua_pushinteger(L, ret);
}

template<>
void lua_tinker::push(lua_State *L, short ret)
{
    lua_pushinteger(L, ret);
}

template<>
void lua_tinker::push(lua_State *L, unsigned short ret)
{
    lua_pushinteger(L, ret);
}

template<>
void lua_tinker::push(lua_State *L, long ret)
{
    lua_pushinteger(L, ret);
}

template<>
void lua_tinker::push(lua_State *L, unsigned long ret)
{
    lua_pushinteger(L, ret);
}

template<>
void lua_tinker::push(lua_State *L, int ret)
{
    lua_pushinteger(L, ret);
}

template<>
void lua_tinker::push(lua_State *L, unsigned int ret)
{
    lua_pushinteger(L, ret);
}

template<>
void lua_tinker::push(lua_State *L, float ret)
{
    lua_pushnumber(L, ret);
}

template<>
void lua_tinker::push(lua_State *L, double ret)
{
    lua_pushnumber(L, ret);
}

template<>
void lua_tinker::push(lua_State *L, char* ret)
{
    lua_pushstring(L, ret);
}

template<>
void lua_tinker::push(lua_State *L, const char* ret)
{
    lua_pushstring(L, ret);
}

template<>
void lua_tinker::push(lua_State *L, std::string ret)
{
    lua_pushlstring(L, ret.c_str(), ret.size());
}

template<>
void lua_tinker::push(lua_State *L, const std::string& ret)
{
    lua_pushlstring(L, ret.c_str(), ret.size());
}

template<>
void lua_tinker::push(lua_State *L, std::string&& ret)
{
    lua_pushlstring(L, ret.c_str(), ret.size());
}

template<>
void lua_tinker::push(lua_State *L, bool ret)
{
    lua_pushboolean(L, ret);
}

template<>
void lua_tinker::push(lua_State *L, lua_value* ret)
{
    if (ret) ret->to_lua(L); else lua_pushnil(L);
}

template<>
void lua_tinker::push(lua_State *L, long long ret)
{
#if(LUA_VERSION_NUM == 501)
    *(long long*)lua_newuserdata(L, sizeof(long long)) = ret;
    lua_pushstring(L, "__s64");
    lua_gettable(L, LUA_GLOBALSINDEX);
    lua_setmetatable(L, -2);
#elif(LUA_VERSION_NUM == 503)
    lua_pushinteger(L, ret);
#endif

}
template<>
void lua_tinker::push(lua_State *L, unsigned long long ret)
{
#if(LUA_VERSION_NUM == 501)
    *(unsigned long long*)lua_newuserdata(L, sizeof(unsigned long long)) = ret;
    lua_pushstring(L, "__u64");
    lua_gettable(L, LUA_GLOBALSINDEX);
    lua_setmetatable(L, -2);
#elif(LUA_VERSION_NUM == 503)
    lua_pushinteger(L, ret);
#endif
}

template<>
void lua_tinker::push(lua_State *L, lua_tinker::table ret)
{
    lua_pushvalue(L, ret.m_obj->m_index);
}

/*---------------------------------------------------------------------------*/
/* pop                                                                       */
/*---------------------------------------------------------------------------*/
template<>
void lua_tinker::pop(lua_State *L)
{
    lua_pop(L, 1);
}

template<>
lua_tinker::table lua_tinker::pop(lua_State *L)
{
    return table(L, lua_gettop(L));
}

/*---------------------------------------------------------------------------*/
/* Tinker Class Helper                                                       */
/*---------------------------------------------------------------------------*/
static void invoke_parent(lua_State *L)
{
    lua_pushstring(L, "__parent");
    lua_rawget(L, -2);
    if (lua_istable(L, -1))
    {
        lua_pushvalue(L, 2);
        lua_rawget(L, -2);
        if (!lua_isnil(L, -1))
        {
            lua_remove(L, -2);
        }
        else
        {
            lua_remove(L, -1);
            invoke_parent(L);
            lua_remove(L, -2);
        }
    }
}

/*---------------------------------------------------------------------------*/
int lua_tinker::meta_get(lua_State *L)
{
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);

    if (lua_isuserdata(L, -1))
    {
        user2type<var_base*>::invoke(L, -1)->get(L);
        lua_remove(L, -2);
    }
    else if (lua_isnil(L, -1))
    {
        lua_remove(L, -1);
        invoke_parent(L);
        if (lua_isnil(L, -1))
        {
            lua_pushfstring(L, "can't find '%s' class variable. (forgot registering class variable ?)", lua_tostring(L, 2));
            lua_error(L);
        }
    }

    lua_remove(L, -2);

    return 1;
}

/*---------------------------------------------------------------------------*/
int lua_tinker::meta_set(lua_State *L)
{
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);

    if (lua_isuserdata(L, -1))
    {
        user2type<var_base*>::invoke(L, -1)->set(L);
    }
    else if (lua_isnil(L, -1))
    {
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
        lua_rawset(L, -4);
    }
    lua_settop(L, 3);
    return 0;
}

/*---------------------------------------------------------------------------*/
void lua_tinker::push_meta(lua_State *L, const char* name)
{
#if (LUA_VERSION_NUM == 501)
    lua_pushstring(L, name);
    lua_gettable(L, LUA_GLOBALSINDEX);
#elif (LUA_VERSION_NUM == 503)
    lua_getglobal(L, name);
#endif
}

/*---------------------------------------------------------------------------*/
/* table object on stack                                                     */
/*---------------------------------------------------------------------------*/
lua_tinker::table_obj::table_obj(lua_State* L, int index)
    :m_L(L)
    , m_index(index)
    , m_ref(0)
{
    if (lua_isnil(m_L, m_index))
    {
        m_pointer = NULL;
        lua_remove(m_L, m_index);
    }
    else
    {
        m_pointer = lua_topointer(m_L, m_index);
    }
}

lua_tinker::table_obj::~table_obj()
{
    if (validate())
    {
        lua_remove(m_L, m_index);
    }
}

void lua_tinker::table_obj::inc_ref()
{
    ++m_ref;
}

void lua_tinker::table_obj::dec_ref()
{
    if (--m_ref == 0)
        delete this;
}

bool lua_tinker::table_obj::validate()
{
    if (m_pointer != NULL)
    {
        if (m_pointer == lua_topointer(m_L, m_index))
        {
            return true;
        }
        else
        {
            int top = lua_gettop(m_L);

            for (int i = 1; i <= top; ++i)
            {
                if (m_pointer == lua_topointer(m_L, i))
                {
                    m_index = i;
                    return true;
                }
            }

            m_pointer = NULL;
            return false;
        }
    }
    else
    {
        return false;
    }
}

/*---------------------------------------------------------------------------*/
/* Table Object Holder                                                       */
/*---------------------------------------------------------------------------*/
lua_tinker::table::table(lua_State* L)
{
    lua_newtable(L);

    m_obj = new table_obj(L, lua_gettop(L));

    m_obj->inc_ref();
}

lua_tinker::table::table(lua_State* L, const char* name)
{
#if (LUA_VERSION_NUM == 501)
    lua_pushstring(L, name);
    lua_gettable(L, LUA_GLOBALSINDEX);

    if (lua_istable(L, -1) == 0)
    {
        lua_pop(L, 1);

        lua_newtable(L);
        lua_pushstring(L, name);
        lua_pushvalue(L, -2);
        lua_settable(L, LUA_GLOBALSINDEX);
    }
#elif (LUA_VERSION_NUM == 503)
    lua_getglobal(L, name);

    if (lua_istable(L, -1) == 0)
    {
        lua_pop(L, 1);

        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, name);
    }

#endif

    m_obj = new table_obj(L, lua_gettop(L));
    m_obj->inc_ref();
}

lua_tinker::table::table(lua_State* L, int index)
{
    if (index < 0)
    {
        index = lua_gettop(L) + index + 1;
    }

    m_obj = new table_obj(L, index);

    m_obj->inc_ref();
}

lua_tinker::table::table(const table& input)
{
    m_obj = input.m_obj;

    m_obj->inc_ref();
}

lua_tinker::table::~table()
{
    m_obj->dec_ref();
}

/*---------------------------------------------------------------------------*/
