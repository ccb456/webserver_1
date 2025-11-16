#include "dbConnsPool.h"


DbConnsPool::~DbConnsPool()
{
    m_isRunning = false;

    // 唤醒所有等待的生产者线程和扫描线程
    m_notEmpty.notify_all();

    if(m_produceThread.joinable())
    {
        m_produceThread.join();
    }

    if(m_scannerThread.joinable())
    {
        m_scannerThread.join();
    }

    std::lock_guard<mutex> locker(m_queueMtx);
    MysqlConnection* conn = nullptr;
    while(!m_connQueue.empty())
    {
        conn = m_connQueue.front();
        m_connQueue.pop();
        delete conn;
    }
    m_connCnt = 0;

#ifdef DEBUG
    std::cout << "ConnectionsPool is clsoed..." << std::endl;

#endif

}

DbConnsPool* DbConnsPool::getInstance()
{
    static DbConnsPool pool;
    return &pool;
}

bool DbConnsPool::loadConfigFile()
{
    m_configPath = getConfigPath() + "connPool.conf";
#ifdef DEBUG
    std::cout << "[configParh:] " << m_configPath << std::endl;
#endif
    std::ifstream ifs(m_configPath);

    if(ifs.is_open())
    {
        string line;
        int idx = -1;
        string key;
        string value;
        int endIdx;

        while(std::getline(ifs, line))
        {
            idx = line.find('=');
            if(idx == string::npos)
            {
                continue;
            }

            endIdx = line.find('\n');
            key = line.substr(0, idx);
            value = line.substr(idx + 1, endIdx - idx - 1);

            std::transform(key.begin(), key.end(), key.begin(), ::tolower);

            if(key == "ip")
            {
                m_ip = value;
            }
            else if(key == "user")
            {
                m_user = value;
            }
            else if(key == "password")
            {
                m_passwd = value;
            }
            else if(key == "port")
            {
                m_port = std::stoi(value);
            }
            else if(key == "dbname")
            {
                m_dbname = value;
            }
            else if(key == "minsize")
            {
                m_minSize = std::stoi(value);
            }
            else if(key == "maxsize")
            {
                m_maxSize = std::stoi(value);
            }
            else if(key == "maxidletime")
            {
                m_maxIdleTime = std::stoi(value);
            }
            else if(key == "connectiontimeout")
            {
                m_connTimeout = std::stoi(value);
            }
            else
            {
                continue;
            }

        }

        ifs.close();

        return true;
    }

#ifdef DEBUG
    std::cout << "ConnectionsPool configFile open failed..." << std::endl;

#endif
    
    return false;
}

DbConnsPool::DbConnsPool()
{

#ifdef DEBUG
    std::cout << "DbConnsPool start init..." << std::endl;
    std::cout << "DbConnsPool is loading Config File..." << std::endl;
#endif
    // 加载配置文件
    if(!loadConfigFile())
    {
#ifdef DEBUG
        std::cout << "DbConnsPool failed to load configuration file..." << std::endl;
#endif
        return;
    }

    m_isRunning = true;

    for(int i = 0; i < m_minSize; ++i)
    {
        MysqlConnection* conn = new MysqlConnection;
        if(conn->connect(m_ip, m_user, m_passwd, m_port, m_dbname))
        {
            conn->refreshAliveTime();
            m_connQueue.push(conn);
            ++m_connCnt;
        }
        else
        {
            return;
        }
    }

    m_produceThread = std::thread(std::bind(&DbConnsPool::produceConnTask, this));
    m_scannerThread = std::thread(std::bind(&DbConnsPool::scannerConnTask, this));

#ifdef DEBUG
        std::cout << "DbConnsPool initialization successful ..." << std::endl;
#endif

}

void DbConnsPool::produceConnTask()
{
    while(m_isRunning)
    {
        std::unique_lock<mutex> locker(m_queueMtx);

        while(!m_connQueue.empty() && m_isRunning)
        {
            m_notEmpty.wait(locker);        // 队列不空，生产者线程进入等待状态
        }

        if(!m_isRunning) break;

        // 队列空了，创建新的连接
        if(m_connCnt < m_maxSize)
        {
            MysqlConnection* conn = new MysqlConnection;
            if(conn->connect(m_ip, m_user, m_passwd, m_port, m_dbname))
            {
                conn->refreshAliveTime();
                m_connQueue.push(conn);
                ++m_connCnt;
                m_notEmpty.notify_one();
            }
            else
            {
                return;
            }

        }
    }
}

shared_ptr<MysqlConnection> DbConnsPool::getConnection()
{
    std::unique_lock<mutex> locker(m_queueMtx);


    // 连接池空闲连接现在为空
    while(m_connQueue.empty())
    {
        if(std::cv_status::timeout == m_notEmpty.wait_for(locker, std::chrono::milliseconds(m_connTimeout)))
        {
            if(m_connQueue.empty())
            {
#ifdef DEBUG
                std::cout << "获取空闲连接超时了......" << std::endl;
#endif
                return nullptr;
            }
        }
    }

    // 连接池不为空
    shared_ptr<MysqlConnection> sp(m_connQueue.front(), [this](MysqlConnection* conn){
        std::unique_lock<mutex> locker(m_queueMtx);
        if(m_isRunning)
        {
            m_connQueue.push(conn);
        }
        else
        {
            delete conn;
        }
    });

    m_connQueue.pop();

    // 谁消费了最后一个连接，谁去唤醒生产者线程
    if(m_connQueue.empty())
    {
        m_notEmpty.notify_all();
    }

    return sp;

}

void DbConnsPool::scannerConnTask()
{
    while(m_isRunning)
    {
        std::this_thread::sleep_for(std::chrono::seconds(m_maxIdleTime));
        std::unique_lock<mutex> locker(m_queueMtx);

        while(m_connCnt > m_minSize)
        {
            MysqlConnection* p = m_connQueue.front();
            if(p->getAliveTime() >= (m_maxIdleTime * 1000))
            {
                m_connQueue.pop();
                --m_connCnt;
                delete p;
            }
            else
            {
                break;
            }
        }
    }
}