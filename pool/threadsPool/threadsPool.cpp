#include "threadsPool.h"

string ThreadsPool::getConfigPath()
{
    // 1. 将程序路径转换为绝对路径（如：/home/ccb/Code/project/webserver/build）
    char exePath[1024];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len == -1) 
    {
#ifdef debug
    std::cout << "ThreadsPool getConfigPath readlink failed..." << std::endl;

#endif
        perror("readlink failed");
        exit(EXIT_FAILURE);
    }
    exePath[len] = '\0';

    string root(exePath);

    // 2. 剔除路径中的"build"目录（关键步骤）
    size_t buildPos = root.find_last_of("/");
    if (buildPos != std::string::npos) 
    {
        // 截取到"build"的前一级目录（如：/home/ccb/Code/project/webserver）
        root = root.substr(0, buildPos);
    }

    // 3. 拼接资源目录（最终：/home/ccb/Code/project/webserver/resources）
    root += "/config/threadPool.conf";

    return root;
}


bool ThreadsPool::loadConfigFile()
{
    m_configPath = getConfigPath();
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

            if(key == "minnum")
            {
                m_minNum = std::stoi(value);
            }
            else if(key == "maxnum")
            {
                m_maxNum = std::stoi(value);
            }
            else if(key == "step")
            {
                m_step = std::stoi(value);
            }
            else
            {
                continue;
            }

        }

        ifs.close();

        return true;
    }

#ifdef debug
    std::cout << "ConnectionsPool configFile open failed..." << std::endl;

#endif
    
    return false;
}


ThreadsPool::ThreadsPool()
:m_busy(0), m_alive(0),m_configPath(""),m_exitCnt(0),m_isStop(false),m_maxNum(0),m_minNum(0),m_step(0)
{

    #ifdef debug
    std::cout << "ThreadsPool start init..." << std::endl;
    std::cout << "ThreadsPool is loading Config File..." << std::endl;
#endif
    // 加载配置文件
    if(!loadConfigFile())
    {
#ifdef debug
        std::cout << "ThreadsPool failed to load configuration file..." << std::endl;
#endif
        return;
    }

    for(int i = 0; i < m_minNum; ++i)
    {
        std::thread(std::bind(&ThreadsPool::work, this)).detach();
    }

    m_mangerThread = std::thread(std::bind(&ThreadsPool::manager, this));

#ifdef debug
        std::cout << "ThreadsPool is running..." << std::endl;
#endif
}

ThreadsPool::~ThreadsPool()
{
    m_isStop.store(true);

    // 唤醒worker
    m_notEmpty.notify_all();

    // 唤醒manager（manager 使用 wait_for，但也可能在等待）
    m_mangerCV.notify_one();

    if(m_mangerThread.joinable())
    {
        m_mangerThread.join();
    }

    // 等待所有worker退出，监视alive = 0；
    {
        std::unique_lock<std::mutex> lk(m_queueMtx);
        m_mangerCV.wait(lk, [&]{return m_alive.load() == 0;});
    }

    // 清理任务队列
    {
        std::unique_lock<std::mutex> lk(m_queueMtx);
        while(!m_tasksQue.empty()) m_tasksQue.pop();
    }

#ifdef debug
    std::cout << "ThreadPool destructed: all workers exited\n";
#endif

}


bool ThreadsPool::addTask(cb_fun task)
{
    if(m_isStop.load()) return false;
    {
        std::lock_guard<std::mutex> lk(m_queueMtx);
        m_tasksQue.push(std::move(task));
    }

    m_notEmpty.notify_one();
    return true;
}

int ThreadsPool::getTaskCount()
{
    std::lock_guard<std::mutex> lk(m_queueMtx);
    return static_cast<int>(m_tasksQue.size());
}

int ThreadsPool::getAliveCount()
{
    return m_alive.load();
}

int ThreadsPool::getBuysCount()
{
    return m_busy.load();
}

int ThreadsPool::getExitCount()
{
    return m_exitCnt.load();
}

bool ThreadsPool::tryConsumeExit()
{
    int v = m_exitCnt.load();
    while (v > 0) 
    {
        if (m_exitCnt.compare_exchange_weak(v, v - 1)) 
        {
            return true;
        }
        // else v updated, loop
    }
    return false;
}

void ThreadsPool::work()
{
    m_alive.fetch_add(1);
#ifdef debug
    std::cout << "worker start, id=" << std::this_thread::get_id() << "\n";
#endif

    while (true) 
    {
        std::function<void()> task;

        {   // 获取任务的临界区
            std::unique_lock<std::mutex> lk(m_queueMtx);

            // 等待条件：队列非空 或 stop 或 有 exit 请求
            m_notEmpty.wait(lk, [&] {
                return !m_tasksQue.empty() || m_isStop.load() || m_exitCnt.load() > 0;
            });

            // 优先响应停止：若 stop 且 无任务，则退出
            if (m_isStop.load() && m_tasksQue.empty()) {
#ifdef debug
                std::cout << "worker stopping (stop flag), id=" << std::this_thread::get_id() << "\n";
#endif
                break;
            }

            // 若有 exit 请求并且当前 alive > minNum，则尝试消费一个 exit token 并退出
            if (m_exitCnt.load() > 0 && m_alive.load() > m_minNum) 
            {
                if (tryConsumeExit()) 
                {
                    // 成功消费 exit token，准备退出（不要在这里直接 decrement alive；统一在退出处 decrement）
#ifdef debug
                    std::cout << "worker exiting due to shrink request, id=" << std::this_thread::get_id() << "\n";
#endif
                    break;
                }
                // 如果未成功消费（被其他线程消费），继续判断任务
            }

            // 取任务
            if (!m_tasksQue.empty()) 
            {
                task = std::move(m_tasksQue.front());
                m_tasksQue.pop();
            } 
            else 
            {
                // 没有任务（可能是 shrink/stop 引起的），继续循环以响应停止/缩容
                continue;
            }
        } // 解锁 queueMutex

        // 执行任务（busy 增/减）
        m_busy.fetch_add(1);
        try 
        {
            task();
        } 
        catch (...) 
        {
            // 捕获任务异常，避免线程崩溃
#ifdef debug
            std::cerr << "Task threw an exception in thread " << std::this_thread::get_id() << "\n";
#endif
        }
        m_busy.fetch_sub(1);
    } // while

    // 线程退出前统一 decrement alive 并通知可能等待的析构/管理线程
    m_alive.fetch_sub(1);
    m_mangerCV.notify_one();

#ifdef debug
    std::cout << "worker exit, id=" << std::this_thread::get_id() << "\n";
#endif

}

void ThreadsPool::manager()
{
#ifdef debug
    std::cout << "manager start\n";
#endif

    while (!m_isStop.load()) 
    {
        // 每次等待 3s（或被显式 notify）来检查扩/缩容
        std::unique_lock<std::mutex> lk(m_queueMtx);
        m_mangerCV.wait_for(lk, std::chrono::seconds(3));

        if (m_isStop.load()) break;

        int qsize = static_cast<int>(m_tasksQue.size());
        int busyCount = m_busy.load();
        int aliveCount = m_alive.load();

        // 扩容条件：任务数 > busy 且 alive < maxNum
        if (qsize > busyCount && aliveCount < m_maxNum) 
        {
            int canAdd = std::min(m_step, m_maxNum - aliveCount);
            for (int i = 0; i < canAdd; ++i) 
            {
                // 直接创建并 detach worker；worker 会在入口处自增 alive
                std::thread(std::bind(&ThreadsPool::work, this)).detach();
            }
#ifdef debug
            std::cout << "manager added " << canAdd << " workers, alive now ~ " << m_alive.load() << "\n";
#endif
        }
        // 缩容条件：工作线程过多（busy * 2 < alive）且 alive > minNum
        else if (busyCount * 2 < aliveCount && aliveCount > m_minNum) 
        {
            int canRemove = std::min(m_step, aliveCount - m_minNum);
            // 安全增加 exitCount（线程见到后会退出）
            m_exitCnt.fetch_add(canRemove);
            // 通知 worker 去消费 exit token（notify_all 更保险）
            m_notEmpty.notify_all();
#ifdef debug
            std::cout << "manager requested shrink by " << canRemove << ", exitCount=" << exitCount.load() << "\n";
#endif
        }
        // else 状态稳定，下一轮检查
    }

#ifdef debug
    std::cout << "manager exit\n";
#endif  
}