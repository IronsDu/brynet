#include <string>
#include <iostream>
#include <unordered_map>
#include <memory>
#include <queue>
#include <assert.h>
#include <functional>
#include <sstream>
#include <chrono>

#include "fdset.h"

#include "libpq-events.h"
#include "libpq-fe.h"
#include "libpq/libpq-fs.h"

using namespace std;

class AsyncPGClient
{
public:
    /*TODO::传递错误信息*/
    typedef std::function<void(const PGresult*)> RESULT_CALLBACK;
    typedef std::function<void(bool value)> BOOL_RESULT_CALLBACK;
    typedef std::function<void(const string& value)> STRING_RESULT_CALLBACK;
    typedef std::function<void(const std::unordered_map<string, string>& value)> STRINGMAP_RESULT_CALLBACK;

    AsyncPGClient() : mKVTableName("kv_data"), mHashTableName("hashmap_data")
    {
        mfdset = ox_fdset_new();
    }

    ~AsyncPGClient()
    {
        for (auto& kv : mConnections)
        {
            PQfinish((*kv.second).pgconn);
        }

        ox_fdset_delete(mfdset);
        mfdset = nullptr;
    }

    void    get(const string& key, const STRING_RESULT_CALLBACK& callback = nullptr)
    {
        mStringStream << "SELECT key, value FROM public." << mKVTableName << " where key = '" << key << "';";

        postQuery(mStringStream.str(), [callback](const PGresult* result){
            if (callback != nullptr && result != nullptr)
            {
                if (PQntuples(result) == 1 && PQnfields(result) == 2)
                {
                    callback(PQgetvalue(result, 0, 1));
                }
            }
        });
    }

    void    set(const string& key, const string& v, const BOOL_RESULT_CALLBACK& callback = nullptr)
    {
        mStringStream << "INSERT INTO public." << mKVTableName << "(key, value) VALUES('" << key << "', '" << v << "') ON CONFLICT(key) DO UPDATE SET value = EXCLUDED.value;";

        postQuery(mStringStream.str(), [callback](const PGresult* result){
            if (callback != nullptr)
            {
                if (PQresultStatus(result) == PGRES_COMMAND_OK)
                {
                    callback(true);
                }
                else
                {
                    cout << PQresultErrorMessage(result);
                    callback(false);
                }
            }
        });
    }

    void    hget(const string& hashname, const string& key, const STRING_RESULT_CALLBACK& callback = nullptr)
    {
        hmget(hashname, { key }, [callback](const std::unordered_map<string, string>& value){
            if (callback != nullptr && !value.empty())
            {
                callback((*value.begin()).second);
            }
        });
    }

    void    hmget(const string& hashname, const std::vector<string>& keys, const STRINGMAP_RESULT_CALLBACK& callback = nullptr)
    {
        mStringStream << "SELECT key, value FROM public." << mHashTableName << " where ";
        auto it = keys.begin();
        do
        {
            mStringStream << "key='" << (*it) << "'";

            ++it;
        } while (it != keys.end() && &(mStringStream << " or ") != nullptr);
        mStringStream << ";";

        postQuery(mStringStream.str(), [callback](const PGresult* result){
            if (callback != nullptr)
            {
                std::unordered_map<string, string> ret;
                if (PQresultStatus(result) == PGRES_TUPLES_OK)
                {
                    int num = PQntuples(result);
                    int fileds = PQnfields(result);
                    if (fileds == 2)
                    {
                        for (int i = 0; i < num; i++)
                        {
                            ret[PQgetvalue(result, i, 0)] = PQgetvalue(result, i, 1);
                        }
                    }
                }

                callback(ret);
            }
        });
    }

    void    hset(const string& hashname, const string& key, const string& value, const BOOL_RESULT_CALLBACK& callback = nullptr)
    {
        mStringStream << "INSERT INTO public." << mHashTableName << "(hashname, key, value) VALUES('" << hashname << "', '" << key << "', '" << value
            << "') ON CONFLICT (hashname, key) DO UPDATE SET value = EXCLUDED.value;";

        postQuery(mStringStream.str(), [callback](const PGresult* result){
            if (callback != nullptr)
            {
                callback(PQresultStatus(result) == PGRES_COMMAND_OK);
            }
        });
    }

    void  hgetall(const string& hashname, const STRINGMAP_RESULT_CALLBACK& callback = nullptr)
    {
        mStringStream << "SELECT key, value FROM public." << mHashTableName << " where hashname = '" << hashname << "';";
        postQuery(mStringStream.str(), [callback](const PGresult* result){
            if (callback != nullptr)
            {
                std::unordered_map<string, string> ret;
                if (PQresultStatus(result) == PGRES_TUPLES_OK)
                {
                    int num = PQntuples(result);
                    int fileds = PQnfields(result);
                    if (fileds == 2)
                    {
                        for (int i = 0; i < num; i++)
                        {
                            ret[PQgetvalue(result, i, 0)] = PQgetvalue(result, i, 1);
                        }
                    }
                }

                callback(ret);
            }
        });
    }

    void    postQuery(const string&& query, const RESULT_CALLBACK& callback = nullptr)
    {
        mPendingQuery.push({ std::move(query), callback});
        mStringStream.str(std::string());
        mStringStream.clear();
    }

    void    postQuery(const string& query, const RESULT_CALLBACK& callback = nullptr)
    {
        mPendingQuery.push({ query, callback });
        mStringStream.str(std::string());
        mStringStream.clear();
    }

public:
    void    poll(int millSecond)
    {
        ox_fdset_poll(mfdset, millSecond);

        std::vector<int> closeFds;

        for (auto& it : mConnections)
        {
            auto fd = it.first;
            auto connection = it.second;
            auto pgconn = connection->pgconn;

            if (ox_fdset_check(mfdset, fd, ReadCheck))
            {
                if (PQconsumeInput(pgconn) > 0 && PQisBusy(pgconn) == 0)
                {
                    bool successGetResult = false;

                    while (true)
                    {
                        auto result = PQgetResult(pgconn);
                        if (result != nullptr)
                        {
                            successGetResult = true;
                            if (connection->callback != nullptr)
                            {
                                connection->callback(result);
                                connection->callback = nullptr;
                            }
                            PQclear(result);
                        }
                        else
                        {
                            break;
                        }
                    }

                    if (successGetResult)
                    {
                        mIdleConnections.push_back(connection);
                    }
                }

                if (PQstatus(pgconn) == CONNECTION_BAD)
                {
                    closeFds.push_back(fd);
                }
            }

            if (ox_fdset_check(mfdset, fd, WriteCheck))
            {
                if (PQflush(pgconn) == 0)
                {
                    //移除可写检测
                    ox_fdset_del(mfdset, fd, WriteCheck);
                }
            }
        }

        for (auto& v : closeFds)
        {
            removeConnection(v);
        }
    }

    void    trySendPendingQuery()
    {
        while (!mPendingQuery.empty() && !mIdleConnections.empty())
        {
            auto& query = mPendingQuery.front();
            auto& connection = mIdleConnections.front();

            if (PQsendQuery(connection->pgconn, query.request.c_str()) == 0)
            {
                cout << PQerrorMessage(connection->pgconn) << endl;
                if (query.callback != nullptr)
                {
                    query.callback(nullptr);
                }
            }
            else
            {
                ox_fdset_add(mfdset, PQsocket(connection->pgconn), WriteCheck);
                connection->callback = query.callback;
            }

            mPendingQuery.pop();
            mIdleConnections.pop_front();
        }
    }

    size_t  pendingQueryNum() const
    {
        return mPendingQuery.size();
    }

    size_t  getWorkingQuery() const
    {
        return mConnections.size() - mIdleConnections.size();
    }

    void    createConnection(  const char *pghost, const char *pgport,
                        const char *pgoptions, const char *pgtty,
                        const char *dbName, const char *login, const char *pwd,
                        int num)
    {
        for (int i = 0; i < num; i++)
        {
            auto pgconn = PQsetdbLogin(pghost, pgport, pgoptions, pgtty, dbName, login, pwd);
            if (PQstatus(pgconn) == CONNECTION_OK)
            {
                auto connection = std::make_shared<Connection>(pgconn, nullptr);
                mConnections[PQsocket(pgconn)] = connection;
                PQsetnonblocking(pgconn, 1);
                ox_fdset_add(mfdset, PQsocket(pgconn), ReadCheck);
                mIdleConnections.push_back(connection);
            }
            else
            {
                cout << PQerrorMessage(pgconn);
                PQfinish(pgconn);
                pgconn = nullptr;
            }
        }

        if (!mConnections.empty())
        {
            sCreateTable((*mConnections.begin()).second->pgconn, mKVTableName, mHashTableName);
        }
    }

private:
    void    removeConnection(int fd)
    {
        auto it = mConnections.find(fd);
        if (it != mConnections.end())
        {
            auto connection = (*it).second;
            for (auto it = mIdleConnections.begin(); it != mIdleConnections.end(); ++it)
            {
                if ((*it)->pgconn == connection->pgconn)
                {
                    mIdleConnections.erase(it);
                    break;
                }
            }

            ox_fdset_del(mfdset, fd, ReadCheck | WriteCheck);
            PQfinish(connection->pgconn);
            mConnections.erase(fd);
        }
    }

private:
    static  void    sCreateTable(PGconn* conn, const string& kvTableName, const string& hashTableName)
    {
        {
            string query = "CREATE TABLE public.";
            query += kvTableName;
            query += "(key character varying NOT NULL, value json, CONSTRAINT key PRIMARY KEY(key))";
            PGresult* exeResult = PQexec(conn, query.c_str());
            auto status = PQresultStatus(exeResult);
            auto errorStr = PQresultErrorMessage(exeResult);
            PQclear(exeResult);
        }

        {
            string query = "CREATE TABLE public.";
            query += hashTableName;
            query += "(hashname character varying, key character varying, value json, "
                    "CONSTRAINT hk PRIMARY KEY (hashname, key))";
            PGresult* exeResult = PQexec(conn, query.c_str());
            auto status = PQresultStatus(exeResult);
            auto errorStr = PQresultErrorMessage(exeResult);
            PQclear(exeResult);
        }
    }

private:
    struct QueryAndCallback
    {
        std::string request;
        RESULT_CALLBACK  callback;
    };

    struct Connection
    {
        PGconn* pgconn;
        RESULT_CALLBACK callback;

        Connection(PGconn* p, RESULT_CALLBACK c)
        {
            pgconn = p;
            callback = c;
        }
    };

    const string                                    mKVTableName;
    const string                                    mHashTableName;

    stringstream                                    mStringStream;
    fdset_s*                                        mfdset;

    std::unordered_map<int, shared_ptr<Connection>> mConnections;
    std::list<shared_ptr<Connection>>               mIdleConnections;

    std::queue<QueryAndCallback>                    mPendingQuery;

    /*TODO::监听wakeup支持*/
    /*TODO::考虑固定分配connection给某业务*/

    /*TODO::编写储存过程,替换现有的hashtable模拟方式,如循环使用jsonb_set以及 select value->k1, value->k2 from ...*/
    /*TODO::编写储存过程,实现list*/
};

int main()
{
    using std::chrono::system_clock;

    AsyncPGClient asyncClient;
    asyncClient.createConnection("127.0.0.1", "5432", nullptr, nullptr, "postgres", "postgres", "19870323", 8);
    system_clock::time_point startTime = system_clock::now();

    for (int i = 0; i < 5000; i++)
    {
        //ap.postQuery("select * from public.kv_data where key='dd';");
        //ap.postQuery("select * from public.kv_data where key='dodo';");
        //ap.postQuery("insert into public.vs(value) values('{\"xxxx\":1}');");
        asyncClient.postQuery("INSERT INTO public.kv_data(key, value) VALUES ('dd1', '{\"hp\":100000}') "
            " ON CONFLICT (key) DO UPDATE SET value = EXCLUDED.value");
        asyncClient.postQuery("INSERT INTO public.kv_data(key, value) VALUES ('dodo2', '{\"hp\":100000}') "
            " ON CONFLICT (key) DO UPDATE SET value = EXCLUDED.value");
        asyncClient.postQuery("INSERT INTO public.kv_data(key, value) VALUES ('dodo5', '{\"hp\":100000}') "
            " ON CONFLICT (key) DO UPDATE SET value = EXCLUDED.value");
        asyncClient.postQuery("INSERT INTO public.kv_data(key, value) VALUES ('33343', '{\"hp\":100000}') "
            " ON CONFLICT (key) DO UPDATE SET value = EXCLUDED.value");
        asyncClient.postQuery("INSERT INTO public.kv_data(key, value) VALUES ('asegg', '{\"hp\":100000}') "
            " ON CONFLICT (key) DO UPDATE SET value = EXCLUDED.value");
        asyncClient.postQuery("INSERT INTO public.kv_data(key, value) VALUES ('132444tgg', '{\"hp\":100000}') "
            " ON CONFLICT (key) DO UPDATE SET value = EXCLUDED.value");
    }

    asyncClient.postQuery("INSERT INTO public.kv_data(key, value) VALUES ('dodo5', '{\"hp\":100000}') "
        " ON CONFLICT (key) DO UPDATE SET value = EXCLUDED.value", [](const PGresult* result){
        cout << "fuck" << endl;
    });

    asyncClient.get("dd", [](const string& value){
        cout << "get dd : " << value << endl;
    });

    asyncClient.set("dd", "{\"hp\":456}", [](bool isOK){
        cout << "set dd : " << isOK << endl;
    });

    asyncClient.hget("heros:dodo", "hp", [](const string& value){
        cout << "hget heros:dodo:" << value << endl;
    });

    asyncClient.hset("heros:dodo", "hp", "{\"hp\":1}", [](bool isOK){
        cout << "hset heros:dodo:" << isOK << endl;
    });

    asyncClient.hmget("heros:dodo", { "hp", "money" }, [](const unordered_map<string, string>& kvs){
        cout << "hmget:" << endl;
        for (auto& kv : kvs)
        {
            cout << kv.first << " : " << kv.second << endl;
        }
    });

    asyncClient.hgetall("heros:dodo", [](const unordered_map<string, string>& kvs){
        cout << "hgetall:" << endl;
        for (auto& kv : kvs)
        {
            cout << kv.first << " : " << kv.second << endl;
        }
    });

    while (true)
    {
        asyncClient.poll(1);
        asyncClient.trySendPendingQuery();
        if (asyncClient.pendingQueryNum() == 0 && asyncClient.getWorkingQuery() == 0)
        {
            break;
        }
    }

    auto elapsed = system_clock::now() - startTime;
    cout << "cost :" << chrono::duration<double>(elapsed).count() << "s" << endl;
    cout << "enter any key exit" << endl;
    cin.get();
    return 0;
}