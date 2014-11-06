#ifndef _LUA_READTABLE_H
#define _LUA_READTABLE_H
extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "luaconf.h"
};

#include "lua_tinker.h"
#include <string>
#include <map>
using namespace std;

struct msvalue_s
{
    string  _str;
    map<string, msvalue_s*>*    _map;    

    msvalue_s(bool initmap);

    msvalue_s(string str);

    ~msvalue_s();
};

void    readluatable(lua_State* l, msvalue_s* outmap);
void    aux_readluatable_byname(lua_State* l, const char* tablename, msvalue_s* outmap);

/*  example */
//--宠物属性重置消耗元宝数
//    pet_refresh_config = {
//        {MONEY_GOLD, 100000, 700, 900},
//        {MONEY_DIAMOND, 20, 700, 900},
//        {MONEY_DIAMOND, 50, 800, 1000},
//}

/*
msvalue_s msvalue(true);
aux_readluatable_byname(global_lua_get(), "pet_refresh_config", &msvalue);

for (map<string, msvalue_s*>::iterator it = (*msvalue._map).begin(); it != (*msvalue._map).end(); ++it)
{
    int config_index = atoi(((*it).first).c_str());
    msvalue_s* _mapvalue = (*it).second;
    assert(_mapvalue != NULL);

    if(_mapvalue != NULL)
    {
        map<string, msvalue_s*>& _submapvalue = *_mapvalue->_map;

        int money_type = atoi(_submapvalue["1"]->_str.c_str());
        int money_value = atoi(_submapvalue["2"]->_str.c_str());
        int rand_min = atoi(_submapvalue["3"]->_str.c_str());
        int rand_max = atoi(_submapvalue["4"]->_str.c_str());

        PetConfigMgr::GetInstance()->addRefreshConfigData(config_index, (MONEY_TYPE)money_type, money_value, rand_min, rand_max);
    }
}
*/

#endif
