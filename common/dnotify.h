#ifndef _DNOTIFY_H
#define _DNOTIFY_H

#include <iostream>
#include <string>
#include <map>
#include <list>
#include <vector>
using namespace  std;

// 任意数据类型
class Value
{
public:
    Value()
    {
        type = 0;
        data = 0;
    }

    int type;
    void* data;
};

typedef int KEY_TYPE;

// 属性集合
template<typename K, typename V>
class PropertyBank
{
private:
    typedef map<K,V> DATAS_TYPE;
public:
    // 添加属性
    void        addProperty(K kvalue, V vvalue)
    {
        m_Propertys[kvalue] = vvalue;
    }

    // 删除属性
    void        delProperty(K kvalue)
    {
        m_Propertys.erase(kvalue);
    }

    // 获取属性
    V*          getProperty(K kvalue)
    {
        V* ret = NULL;
        typename DATAS_TYPE::iterator it = m_Propertys.find(kvalue);
        if(it != m_Propertys.end())
        {
            ret = &(it->second);
        }

        return ret;
    }

    V*          operator [](K kvalue)
    {
        return m_Propertys[kvalue];
    }

private:
    DATAS_TYPE    m_Propertys;
};

// K为int的属性集合偏特化
template<typename V>
class PropertyBank<int, V>
{
public:
    void        addProperty(int kvalue, V vvalue)
    {
        if (kvalue >= (int)m_Propertys.size())
        {
            m_Propertys.resize(kvalue+1);
        }

        m_Propertys[kvalue] = vvalue;
    }

    void        delProperty(int kvalue)
    {
        if (kvalue < (int)m_Propertys.size())
        {
            m_Propertys[kvalue] = V();
        }
    }

    V*          getProperty(int kvalue)
    {
        V* ret = NULL;
        if (kvalue < (int)m_Propertys.size())
        {
            ret = &(m_Propertys[kvalue]);
        }

        return ret;
    }

    V*          operator [](int kvalue)
    {
        return getProperty(kvalue);
    }

private:
    vector<V>    m_Propertys;
};

// HasProperty为[int-Value*]属性集合
class HasProperty : public PropertyBank<KEY_TYPE, Value*>
{
};

class Entity;

class HasEntity
{
public:
    HasEntity() : m_entity(NULL){}
protected:
    Entity* getEntity()
    {
        return m_entity;
    }
    void    setEntity(Entity* pEntity)
    {
        m_entity = pEntity;
    }
private:
    Entity* m_entity;

    friend class Entity;
};

enum ELOGIC_CHECK_TYPE
{
    ELOGIC_CHECK_NONE,
    ELOGIC_CHECK_OK,
    ELOGIC_CHECK_NO,
};

typedef ELOGIC_CHECK_TYPE    (*notify_handler_pt)(void* reg_arg, void* run_arg);
struct handler_s 
{
    int id;
    notify_handler_pt callback;
    void*   reg_arg;
};

// 逻辑列表,同样的k对应的属性不是单独的,而是列表
class LogicCollection
{
protected:
    typedef list<handler_s*> LIST_LOGIC;

public:
    // 添加逻辑
    void    addLogic(KEY_TYPE key, handler_s* handler)
    {
        LIST_LOGIC** pplist = m_logics[key];
        LIST_LOGIC* list = NULL;

        if(NULL == pplist)
        {
            list = new LIST_LOGIC;
            if(NULL != list)
            {
                list->push_back(handler);
                m_logics.addProperty(key, list);
            }
        }
        else
        {
            list = *pplist;
            list->push_back(handler);
        }
    }
    // 删除逻辑
    void    delLogic(KEY_TYPE key, int id)
    {
        LIST_LOGIC** pplist= m_logics.getProperty(key);
        if(NULL == pplist)
        {
            return;
        }

        LIST_LOGIC* list = *pplist;
        if(NULL != list)
        {
            for(LIST_LOGIC::iterator it = list->begin(); it != list->end(); ++it)
            {
                if((*it)->id == id)
                {
                    list->erase(it);
                    break;
                }
            }
        }
    }

    // 执行逻辑
    void    execute(KEY_TYPE key, void* run_arg)
    {
        LIST_LOGIC** pplist = m_logics.getProperty(key);
        if(NULL == pplist)
        {
            return;
        }

        LIST_LOGIC* list = *pplist;
        if(NULL != list)
        {
            for(LIST_LOGIC::iterator it = list->begin(); it != list->end(); ++it)
            {
                handler_s* handler = *it;
                handler->callback(handler->reg_arg, run_arg);
            }
        }
    }

    // 执行逻辑检测
    ELOGIC_CHECK_TYPE   check(KEY_TYPE key, void* run_arg)
    {
        ELOGIC_CHECK_TYPE retValue = ELOGIC_CHECK_NONE;
        LIST_LOGIC** pplist = m_logics.getProperty(key);
        if(NULL != pplist)
        {
            LIST_LOGIC* list = *pplist;
            if(NULL != list)
            {
                for(LIST_LOGIC::iterator it = list->begin(); it != list->end(); ++it)
                {
                    // 遇到一个检测逻辑返回失败,退出检测
                    handler_s* handler = *it;
                    retValue = handler->callback(handler->reg_arg, run_arg);
                    if(retValue == ELOGIC_CHECK_NO)
                    {
                        break;
                    }
                }
            }
        }

        return retValue;
    }

private:
    PropertyBank<KEY_TYPE, LIST_LOGIC*>  m_logics;
};

// 实体类:具有属性表,内涵逻辑列表、观察者列表、检测者列表
class Entity : public HasProperty 
{
public:
    void    addLogic(KEY_TYPE key, handler_s* handler)
    {
        m_logicCollection.addLogic(key, handler);
    }

    void    delLogic(KEY_TYPE key, int id)
    {
        m_logicCollection.delLogic(key, id);
    }

    void    execute(KEY_TYPE key, void* run_arg)
    {
        m_logicCollection.execute(key, run_arg);
    }

    void    addCheck(KEY_TYPE key, handler_s* handler)
    {
        m_checks.addLogic(key, handler);
    }

    void    delCheck(KEY_TYPE key, int id)
    {
        m_checks.delLogic(key, id);
    }

    ELOGIC_CHECK_TYPE   check(KEY_TYPE key, void* run_arg)
    {
        return m_checks.check(key, run_arg);
    }

    void    addObserver(KEY_TYPE key, handler_s* handler)
    {
        m_observers.addLogic(key, handler);
    }

    void    delObserver(KEY_TYPE key, int id)
    {
        m_observers.delLogic(key, id);
    }

    void    notify(KEY_TYPE key, void* run_arg)
    {
        m_observers.execute(key, run_arg);
    }

private:

    LogicCollection m_logicCollection;	// 用于主动执行逻辑
    LogicCollection m_observers;		// 用于被动通知
    LogicCollection m_checks;			// 用于主动检测
};

#endif