#include <assert.h>

#include "lua_readtable.h"

msvalue_s::msvalue_s(bool initmap)
{
    _map = NULL;
    if(initmap)
    {
        _map = new map<string, msvalue_s*>;
    }
}

msvalue_s::msvalue_s(string str)
{
    _map = NULL;
    _str = str;
}

msvalue_s::~msvalue_s()
{
    if(_map != NULL)
    {
        for (map<string, msvalue_s*>::iterator it = _map->begin(); it != _map->end(); ++ it)
        {
            msvalue_s* temp = (*it).second;
            delete temp;
        }

        _map->clear();
        delete _map;
        _map = NULL;
    }
}

void readluatable(lua_State* l, msvalue_s* outmap)
{
    if (!lua_istable(l, -1))
    {
        throw std::runtime_error("table is not found");
    }

    int t_index = lua_gettop(l);

    lua_pushnil(l);
    while(lua_next(l, t_index))
    {
        /*  读入一个元素的操作   */
        /*  把用回调函数处理kv  */
        string k;
        string v;

        int k_type = lua_type(l, -2);

        if(k_type == LUA_TNUMBER)
        {
            lua_Number key_value = lua_tonumber(l, -2);
            k = std::to_string(key_value);
        }
        else if(k_type == LUA_TSTRING)
        {
            k = lua_tostring(l, -2);
        }
        else
        {
            assert(false);
            throw std::runtime_error("key type is not number or string");
        }

        int v_type = lua_type(l, -1);
        if(v_type == LUA_TTABLE)
        {
            (*outmap->_map)[k] = new msvalue_s(true);
            readluatable(l, (*outmap->_map)[k]);
        }
        else if(v_type == LUA_TSTRING)
        {
            v = lua_tostring(l, -1);
            (*outmap->_map)[k] = new msvalue_s(v);
        }
        else if(v_type == LUA_TNUMBER)
        {
            lua_Number v_value = lua_tonumber(l, -1);
            v = std::to_string(v_value);
            (*outmap->_map)[k] = new msvalue_s(v);
        }
        else if (v_type == LUA_TBOOLEAN)
        {
            lua_Number v_value = lua_toboolean(l, -1);
            v = std::to_string(v_value);
            (*outmap->_map)[k] = new msvalue_s(v);
        }
        else
        {
            assert(false);
            throw std::runtime_error("value type is not table , number or string");
        }

        lua_pop(l, 1);
    }
}

void aux_readluatable_byname(lua_State* l, const char* tablename, msvalue_s* outmap)
{
    lua_getglobal(l, tablename);
    if (lua_istable(l, -1))
    {
        readluatable(l, outmap);
    }
    else
    {
        string error = string("table ") + string(tablename) + string("is not found");
        throw std::runtime_error(error.c_str());
    }
}