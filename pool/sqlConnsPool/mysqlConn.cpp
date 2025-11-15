#include "mysqlConn.h"

MysqlConnection::MysqlConnection()
{
    m_mysql = mysql_init(nullptr);
    assert(m_mysql);
}

MysqlConnection::~MysqlConnection()
{
    if(m_mysql != nullptr)
    {
        mysql_close(m_mysql);
    }
}

bool MysqlConnection::connect(const string& ip, const string& user, const string& passwd, const unsigned int port, const string& dbname)
{
    MYSQL* p = mysql_real_connect(m_mysql, ip.c_str(), user.c_str(), passwd.c_str(), dbname.c_str(), port, nullptr, 0);

    return p != nullptr;
}

bool MysqlConnection::update(const string& sql)
{
    if(mysql_query(m_mysql, sql.c_str()))
    {
        return false;
    }

    return true;
}

MYSQL_RES* MysqlConnection::query(const string& sql)
{
    if(mysql_query(m_mysql, sql.c_str()))
    {
        return nullptr;
    }

    return mysql_use_result(m_mysql);
}