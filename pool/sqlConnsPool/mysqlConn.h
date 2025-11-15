#ifndef MYSQLCONN_H
#define MYSQLCONN_H

#include <string>
#include <mysql/mysql.h>
#include <ctime>
#include <assert.h>

using std::string;

class MysqlConnection
{
public:
    MysqlConnection();
    ~MysqlConnection();

    bool connect(const string& ip, const string& user, const string& passwd, const unsigned int port, const string& dbname);

    bool update(const string& sql);

    MYSQL_RES* query(const string& sql);

    void refreshAliveTime()
    {
        m_aliveTime = clock();
    }

    clock_t getAliveTime() const
    {
        return clock() - m_aliveTime;
    }


private:
    MYSQL* m_mysql;
    clock_t m_aliveTime;    //记录进入空闲状态后的起始存活时间
};

#endif