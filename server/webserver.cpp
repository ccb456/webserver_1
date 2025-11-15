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

void Webserver::sendError(int fd, const char* info)
{
    assert(fd > 0);
    int len = send(fd, info, strlen(info), 0);
    if(len < 0)
    {

    } 
    close(fd);
}

void Webserver::closeConn(HttpConn* client)
{
    assert(client);
    m_epoller->removeFd(client->getFd());
    client->closeConn();
}

void Webserver::addClnt(int fd, sockaddr_in addr)
{
    assert(fd > 0);
    m_users[fd].init(fd, addr);
    if(m_timeout > 0)
    {
        // 添加定时器
    }

    m_epoller->addFd(fd, EPOLLIN | m_clntEvent);
    setnoblock(fd);
}

void Webserver::dealListen()
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    do
    {
        int fd = accept(m_listenFd, (struct sockaddr*)&addr, &len);
        if(fd < 0)
        {
            return;
        }
        else if(HttpConn::userCount >= MAX_FD)
        {
            sendError(fd, "Server busy!");
            return;
        }

        addClnt(fd, addr);

    } while (m_listenEvent & EPOLLET);
    
}

void Webserver::dealRead(HttpConn* client)
{
    assert(client);
    extentTime(client);

    // 线程池添加任务
}

void Webserver::dealWrite(HttpConn* client)
{
    assert(client);
    extentTime(client);
    // 线程池添加任务
}

void Webserver::extentTime(HttpConn* client)
{
    assert(client);
    if(m_timeout > 0)
    {
        // 调整时间
    }
}

void Webserver::onRead(HttpConn* client)
{
    assert(client);
    int ret = -1;
    int readErrno = 0;

    ret = client->readFromClnt(&readErrno);

    if(ret < 0 && readErrno != EAGAIN)
    {
        closeConn(client);

        return;
    }

    onProcess(client);
}

void Webserver::onProcess(HttpConn* client)
{
    if(client->process())
    {
        m_epoller->modFd(client->getFd(), m_clntEvent | EPOLLOUT);
    }
    else
    {
        m_epoller->modFd(client->getFd(), m_clntEvent | EPOLLIN);
    }
}

void Webserver::onWrite(HttpConn* client)
{
    assert(client);
    int ret = -1;
    int writeErrno = 0;

    ret = client->writeToClnt(&writeErrno);
    if(client->toWriteBytes() == 0)
    {
        /* 传输完成 */
        if(client->isKeepAlive())
        {
            onProcess(client);
            return;
        }
    }
    else if(ret < 0)
    {
        if(writeErrno == EAGAIN)
        {
            m_epoller->modFd(client->getFd(), m_clntEvent | EPOLLOUT);
            return;
        }
    }

    closeConn(client);
}

bool Webserver::initSocket()
{
    int ret;
    struct sockaddr_in addr;

    if(m_port > 65535 || m_port < 1024)
    {
        return false;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(m_port);

    struct linger optLinger = { 0 };
    if(m_openLinger)
    {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if(m_listenFd < 0)
    {
        return false;
    }

    ret = setsockopt(m_listenFd, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0)
    {
        close(m_listenFd);
        return false;
    }

    ret = bind(m_listenFd, (struct sockaddr*)&addr, sizeof(addr));
    if(ret < 0)
    {
        close(m_listenFd);
        return false;
    }

    ret = listen(m_listenFd, 6);
    if(ret < 0)
    {
        close(m_listenFd);
        return false;
    }

    ret = m_epoller->addFd(m_listenFd, m_listenEvent | EPOLLIN);
    if(ret == 0)
    {
        close(m_listenFd);
        return false;
    }

    setnoblock(m_listenFd);
    return true;
}

int Webserver::setnoblock(int fd)
{
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}