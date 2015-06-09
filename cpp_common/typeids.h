#ifndef _TYPEIDS_H
#define _TYPEIDS_H

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
            ret = mIds[mIds.size() - 1];
            mIds.pop_back();
        }

        return ret;
    }

    void        reclaimID(size_t id)
    {
        assert(id >= 0 && id < mValues.size());
        mIds.push_back(id);
    }

    bool        set(T t, size_t id)
    {
        assert(id >= 0 && id < mValues.size());
        if (id >= 0 && id < mValues.size())
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
        assert(id >= 0 && id < mValues.size());
        if (id >= 0 && id < mValues.size())
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
        const static int _NUM = 100;

        int oldsize = mValues.size();
        if (oldsize > 0)
        {
            mValues.resize(oldsize + _NUM, nullptr);
            for (int i = 0; i < _NUM; i++)
            {
                mIds.push_back(oldsize + i);
            }
        }
        else
        {
            mValues.resize(oldsize + _NUM, nullptr);
            for (int i = 0; i < _NUM; i++)
            {
                mIds.push_back(oldsize + i);
            }
        }
    }

private:
    std::vector<T>      mValues;
    std::vector<size_t> mIds;
};

#endif