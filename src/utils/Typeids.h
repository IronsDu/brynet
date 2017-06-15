#ifndef BRYNET_TYPEIDS_H_
#define BRYNET_TYPEIDS_H_

#include <vector>
#include <cassert>
#include <exception>
#include <algorithm>

/*采用ID管理对象的管理器*/

namespace brynet
{
    template<typename T>
    class TypeIDS
    {
    public:
        size_t         claimID()
        {
            size_t ret = 0;

            if (mIds.empty())
            {
                increase();
            }

            if (mIds.empty())
            {
                throw std::runtime_error("no memory in TypeIDS::claimID");
            }

            ret = mIds[mIds.size() - 1];
            mIds.pop_back();

            return ret;
        }

        void        reclaimID(size_t id)
        {
            assert(id < mValues.size());
            assert(std::find(mIds.begin(), mIds.end(), id) == mIds.end());
            mIds.push_back(id);
        }

        bool        set(const T& t, size_t id)
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

        bool        set(T&& t, size_t id)
        {
            assert(id < mValues.size());
            if (id < mValues.size())
            {
                mValues[id] = std::move(t);
                return true;
            }
            else
            {
                return false;
            }
        }

        bool          get(size_t id, T& out) const
        {
            assert(id < mValues.size());
            if (id < mValues.size())
            {
                out = std::move(mValues[id]);
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
}

#endif