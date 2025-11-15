#include "webserver.h"

void Webserver::setResourcePath()
{
    // 1. 将程序路径转换为绝对路径（如：/home/ccb/Code/project/webserver/build）
    char exePath[1024];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len == -1) 
    {
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
    root += "/resources/";

    m_srcDir = root;
}


Webserver::Webserver(int port, int trigMode, int timeoutMS, bool optLinger)
:m_port(port),m_timeout(timeoutMS), m_openLinger(optLinger)
{
    setResourcePath();

    HttpConn::userCount = 0;
    HttpConn::srcDir = m_srcDir.c_str();

    initEventMode(trigMode);

    if(!initSocket())
    {
        m_isClose = true;
    }

}

Webserver::~Webserver()
{
    close(m_listenFd);
    m_isClose = true;
}

void Webserver::initEventMode(int trigMode)
{
    m_listenEvent = EPOLLRDHUP;
    m_clntEvent = EPOLLONESHOT | EPOLLRDHUP;

    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        m_clntEvent |= EPOLLET;
        break;
    case 2:
        m_listenEvent |= EPOLLET;
        break;
    case 3:
        m_listenEvent |= EPOLLET;
        m_clntEvent |= EPOLLET;
        break;
    
    default:
        m_listenEvent |= EPOLLET;
        m_clntEvent |= EPOLLET;
        break;
    }

    HttpConn::isET = (m_clntEvent & EPOLLET);
}

void Webserver::run()
{
    int timeMS = -1;

    while(!m_isClose)
    {
        if(m_timeout > 0)
        {
            // timeMS 得到下一个超时时间
        }

        int eventCnt = m_epoller->wait(timeMS);
        for(int i = 0; i < eventCnt; ++i)
        {
            int sockfd = m_epoller->getEventFd(i);
            uint32_t events = m_epoller->getEvents(i);

            if(sockfd == m_listenFd)
            {
                dealListen();
            }
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                assert(m_users.count(sockfd) > 0);
                closeConn(&m_users[sockfd]);
            }
            else if(events & EPOLLIN)
            {
                assert(m_users.count(sockfd) > 0);
                dealRead(&m_users[sockfd]);
            }
            else if(events & EPOLLOUT)
            {
                assert(m_users.count(sockfd) > 0);
                dealWrite(&m_users[sockfd]);
            }
            else
            {
                
            }
        }
    }
}