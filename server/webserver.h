#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unordered_map>

#include "epoller.h"
#include "../http/httpConn.h"
#include "../timer/minHeapTimer.h"
#include "../pool/sqlConnsPool/dbConnsPool.h"
#include "../pool/threadsPool/threadsPool.h"


class Webserver
{
public:
    Webserver(int port, int trigMode, int timeoutMS, bool optLinger);
    ~Webserver();
    void run();

private:
    static const int MAX_FD = 65536;

    static int setnoblock(int fd);

    bool initSocket();
    void initEventMode(int trigMode);
    void addClnt(int fd, sockaddr_in addr);

    void dealListen();
    void dealWrite(HttpConn* client);
    void dealRead(HttpConn* client);

    void sendError(int fd, const char* info);
    void extentTime(HttpConn* client);
    void closeConn(HttpConn* client);

    void onRead(HttpConn* client);
    void onWrite(HttpConn* client);
    void onProcess(HttpConn* client);

    void setResourcePath();


private:
    int m_port;
    bool m_openLinger;  // 是否启用优雅关闭（SO_LINGER），确保连接关闭前发送完剩余数据。
    int m_timeout;
    bool m_isClose;

    int m_listenFd;
    string m_srcDir;

    uint32_t m_listenEvent;     // 监听套接字的 epoll 事件类型（如 EPOLLIN、EPOLLET)
    uint32_t m_clntEvent;       // 客户端连接的 epoll 事件类型（如 EPOLLIN、EPOLLOUT、EPOLLET）

    /* 定时器 */
    std::unique_ptr<MinHeapTimer> m_timer;
    /* 线程池 */
    std::unique_ptr<ThreadsPool> m_threadsPool;
    
    std::unique_ptr<Epoller> m_epoller;
    std::unordered_map<int, HttpConn> m_users;      // 客户端连接映射表（fd -> HttpConn 对象）

};

#endif