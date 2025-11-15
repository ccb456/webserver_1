#ifndef THREADSPOOL_H
#define THREADSPOOL_H

#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unistd.h>
#include <thread>
#include <vector>
#include <string>
#include <atomic>
#include <fstream>


using std::cout;
using std::endl;
using std::string;


class ThreadsPool
{
public:
    using cb_fun = std::function<void()>;

public:
    ThreadsPool();
    ~ThreadsPool();

    // 禁止拷贝
    ThreadsPool(const ThreadsPool&) = delete;
    ThreadsPool& operator=(const ThreadsPool&) = delete;

    bool addTask(cb_fun task);
    int getTaskCount();

    int getBuysCount();
    int getAliveCount();
    int getExitCount();

private:
    bool loadConfigFile();
    static string getConfigPath();

    void work();
    void manager();

    bool tryConsumeExit();

private:
    /* 线程池相关参数 */
    int m_maxNum;
    int m_minNum;
    string m_configPath;
    int m_step;
    
    /* 队列相关参数 */
    std::mutex m_queueMtx;
    std::condition_variable m_notEmpty;
    std::queue<cb_fun> m_tasksQue;
    
    /* 线程 */
    std::condition_variable m_mangerCV;      // 用于管理线程/析构等待 alive==0
    std::thread m_mangerThread;
    
    /* 状态 */
    std::atomic<int> m_busy;
    std::atomic<int> m_alive;
    std::atomic<int> m_exitCnt;
    std::atomic<bool> m_isStop;
};

#endif