#ifndef _TYPEIDS_H
#define _TYPEIDS_H

#include <cassert>

/*采用ID管理对象的管理器*/

template<typename T>
class TypeIDS
{
public:
    int         claimID()
    {
        int ret = -1;

        if (mIds.empty())
        {
            increase();
        }

        assert(!mIds.empty());

        if (!mIds.empty())
        {
            ret = static_cast<int>(mIds[mIds.size() - 1]);
            mIds.pop_back();
        }

        return ret;
    }

    void        reclaimID(size_t id)
    {
        assert(id < mValues.size());
        mIds.push_back(id);
    }

    bool        set(T t, size_t id)
    {
        assert(id < mValues.size());
        if (id < mValues.size())
        {
            mValues[id] = t;
            return true;
        }
        else
        {
            return false; 
        }
    }

    bool          get(size_t id, T& out)
    {
        assert(id < mValues.size());
        if (id < mValues.size())
        {
            out = mValues[id];
            return true;
        }
        else
        {
            return false;
        }
    }

private:
    void                increase()
    {
        const static size_t _NUM = 100;

        size_t oldsize = mValues.size();
        mValues.resize(oldsize + _NUM, nullptr);
        for (size_t i = 0; i < _NUM; i++)
        {
            mIds.push_back(oldsize + i);
        }
    }

private:
    std::vector<T>      mValues;
    std::vector<size_t> mIds;
};

#endif