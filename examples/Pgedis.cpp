#include <string>
#include <iostream>
#include <unordered_map>
#include <assert.h>

#include "libpq-events.h"
#include "libpq-fe.h"
#include "libpq/libpq-fs.h"

using namespace std;

class PGedis
{
public:
    PGedis() : mKVTableName("kv_data"), mHashTableName("hashmap_data")
    {
        mConn = nullptr;
    }

    ~PGedis()
    {
        if (mConn != nullptr)
        {
            PQfinish(mConn);
            mConn = nullptr;
        }
    }

    void    connect(const char *pghost, const char *pgport,
                    const char *pgoptions, const char *pgtty,
                    const char *dbName, const char *login, const char *pwd)
    {
        mConn = PQsetdbLogin(pghost, pgport, pgoptions, pgtty, dbName, login, pwd);
        createTable();
    }

    string  get(const string& k)
    {
        string ret;

        const char *paramValues[] = { k.c_str() };
        PGresult* result = PQexecParams(mConn, "SELECT key, value FROM public.kv_data where key = $1; ",
            sizeof(paramValues) / sizeof(paramValues[0]), nullptr, paramValues, nullptr, nullptr, 1);
        auto status = PQresultStatus(result);
        if (status == PGRES_TUPLES_OK)
        {
            int num = PQntuples(result);
            int fileds = PQnfields(result);
            if (num == 1 && fileds == 2)
            {
                ret = PQgetvalue(result, 0, 1);
            }
        }

        PQclear(result);

        return ret;
    }

    bool    set(const string& k, const string& v)
    {
        bool isOK = true;

        const char *paramValues[] = { k.c_str(), v.c_str() };
        PGresult* result = PQexecParams(mConn, "INSERT INTO public.kv_data(key, value) VALUES ($1, $2)"
            " ON CONFLICT (key) DO UPDATE SET value = EXCLUDED.value; ",
            sizeof(paramValues) / sizeof(paramValues[0]), nullptr, paramValues, nullptr, nullptr, 1);
        auto status = PQresultStatus(result);
        if (status != PGRES_COMMAND_OK)
        {
            isOK = false;
            cout << PQresultErrorMessage(result);
        }

        PQclear(result);

        return isOK;
    }

    string hget(const string& hashname, const string& key)
    {
        string ret;

        std::unordered_map<string, string> result = hmget(hashname, { key });
        if (!result.empty())
        {
            ret = (*result.begin()).second;
        }

        return ret;
    }

    std::unordered_map<string, string> hmget(const string& hashname, const std::vector<string>& keys)
    {
        std::unordered_map<string, string> ret;

        string query("SELECT key, value FROM public.hashmap_data where ");
        auto it = keys.begin();
        do 
        {
            query += "key='";
            query += (*it);
            query += "'";

            ++it;
        } while (it != keys.end() && !(query += " or ").empty());

        PGresult* result = PQexec(mConn, query.c_str());
        auto status = PQresultStatus(result);
        if (status == PGRES_TUPLES_OK)
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
        PQclear(result);

        return ret;
    }

    bool    hset(const string& hashname, const string& k, const string& v)
    {
        bool isOK = true;

        const char *paramValues[] = { hashname.c_str(), k.c_str(), v.c_str() };

        PGresult* result = PQexecParams(mConn, "INSERT INTO public.hashmap_data(hashname, key, value) VALUES ($1, $2, $3)"
                                                "ON CONFLICT (hashname,key) DO UPDATE SET value = EXCLUDED.value; ",
                                                sizeof(paramValues)/sizeof(paramValues[0]), nullptr, paramValues, nullptr, nullptr, 1);
        auto status = PQresultStatus(result);
        if (status != PGRES_COMMAND_OK)
        {
            isOK = false;
            cout << PQresultErrorMessage(result);
        }

        PQclear(result);

        return isOK;
    }

    std::unordered_map<string, string>  hgetall(const string& hashname)
    {
        std::unordered_map<string, string> ret;

        const char *paramValues[] = { hashname.c_str() };
        PGresult* result = PQexecParams(mConn, "SELECT key, value FROM public.hashmap_data where hashname = $1; ",
            sizeof(paramValues) / sizeof(paramValues[0]), nullptr, paramValues, nullptr, nullptr, 1);
        auto status = PQresultStatus(result);
        if (status == PGRES_TUPLES_OK)
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

        PQclear(result);

        return ret;
    }

private:
    void    createTable()
    {
        {
            PGresult* exeResult = PQexec(mConn, "CREATE TABLE public.kv_data( key character varying NOT NULL,value json, CONSTRAINT key PRIMARY KEY (key))");
            auto status = PQresultStatus(exeResult);
            auto errorStr = PQresultErrorMessage(exeResult);
            PQclear(exeResult);
        }

        {
            PGresult* exeResult = PQexec(mConn, "CREATE TABLE public.hashmap_data( hashname character varying,key character varying,value json, "
                                                "CONSTRAINT hk PRIMARY KEY (hashname, key))");
            auto status = PQresultStatus(exeResult);
            auto errorStr = PQresultErrorMessage(exeResult);
            PQclear(exeResult);
        }
    }

private:
    PGconn* mConn;
    const string    mKVTableName;
    const string    mHashTableName;

    /*TODO::自定义默认的kv和hash table name*/
    /*TODO::编写储存过程,替换现有的hashtable模拟方式,如循环使用jsonb_set以及 select value->k1, value->k2 from ...*/
    /*TODO::编写储存过程,实现list*/
};

int main()
{
    PGedis pgedis;
    pgedis.connect("127.0.0.1", "5432", nullptr, nullptr, "postgres", "postgres", "19870323");
    pgedis.set("dd", "{}");
    string value = pgedis.get("dd");
    cout << value << endl;

    pgedis.set("dd", "{\"hp\":100000}");
    value = pgedis.get("dd");

    cout << value << endl;

    pgedis.hset("heros:dodo", "hp", "{\"hp\":100000}");
    pgedis.hset("heros:dodo", "hp", "{\"hp\":1}");
    pgedis.hset("heros:dodo", "money", "{\"money\":100000}");

    auto ps = pgedis.hmget("heros:dodo", { "hp", "money" });

    auto hp = pgedis.hget("heros:dodo", "hp");
    auto dodoProperties = pgedis.hgetall("heros:dodo");

    cout << "pause any key" << endl;
    cin.get();
    return 0;
}