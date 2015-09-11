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
            char temp[1024];
            int key_value = lua_tonumber(l, -2);
            sprintf(temp, "%d", key_value);
            k = temp;
        }
        else if(k_type == LUA_TSTRING)
        {
            k = lua_tostring(l, -2);
        }
        else
        {
            assert(false);
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
            char temp[1024];
            int v_value = lua_tonumber(l, -1);
            sprintf(temp, "%d", v_value);
            v = temp;
            (*outmap->_map)[k] = new msvalue_s(v);
        }
        else
        {
            assert(false);
        }

        lua_pop(l, 1);
    }
}

void aux_readluatable_byname(lua_State* l, const char* tablename, msvalue_s* outmap)
{
    lua_getglobal(l, tablename);
    readluatable(l, outmap);
}