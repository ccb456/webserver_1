#ifndef DBCONNSPOOL_H
#define DBCONNSPOOL_H

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <atomic>
#include <memory>
#include <thread>
#include <functional>
#include <unistd.h>
#include <mysqlConn.h>
#include <iostream>
#include <algorithm>

using std::string;
using std::mutex;
using std::queue;
using std::shared_ptr;

class DbConnsPool
{
public:
    static DbConnsPool* getInstance();
    shared_ptr<MysqlConnection> getConnection();
    ~DbConnsPool();

    static string getConfigPath();


private:
    DbConnsPool();
    bool loadConfigFile();

    void produceConnTask();

    void scannerConnTask();


private:
    /* mysql 的连接信息 */
    string m_ip;
    string m_user;
    string m_passwd;
    string m_dbname;
    unsigned int m_port;

    /* 连接池本身的参数 */
    int m_minSize;
    int m_maxSize;
    int m_maxIdleTime;    // 连接池最大空闲时间
    int m_connTimeout;    // 连接池获取连接的超时时间
    string configPath;

    queue<MysqlConnection*> m_connQueue;
    mutex m_queueMtx;

    std::atomic<int> m_connCnt;             // 连接的总数
    std::condition_variable m_notEmpty;
    std::atomic<bool> m_isRunning{true};

    std::thread m_produceThread;
    std::thread m_scannerThread;
};

#endif