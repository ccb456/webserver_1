#include "webserver.h"




Webserver::Webserver(int port, int trigMode, int timeoutMS, bool optLinger)
:m_port(port),m_timeout(timeoutMS), m_openLinger(optLinger),m_timer(new MinHeapTimer()), 
m_threadsPool(new ThreadsPool()), m_epoller(new Epoller()) 
{
    m_srcDir = getSrcPath() + "/resources/";
#ifdef DEBUG
    std::cout << "[src:] " << m_srcDir << std::endl;
#endif

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
    DbConnsPool::getInstance()->~DbConnsPool();
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
            timeMS = m_timer->getNextTick();
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
#ifdef DEBUG
        std::cout << "Event error!" << std::endl;
#endif  
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
#ifdef DEBUG
        std::cout << "send error to client[" << fd  << "]"<< std::endl;
#endif 
    } 
    close(fd);
}

void Webserver::closeConn(HttpConn* client)
{
    assert(client);
    m_epoller->removeFd(client->getFd());
    client->closeConn();
#ifdef DEBUG
        std::cout << "close client [" << client->getFd() << "] connection." << std::endl;
#endif 
}

void Webserver::addClnt(int fd, sockaddr_in addr)
{
    assert(fd > 0);
    m_users[fd].init(fd, addr);
    if(m_timeout > 0)
    {
        // 添加定时器
        m_timer->add(fd, m_timeout, std::bind(&Webserver::closeConn, this, &m_users[fd]));
    }

    m_epoller->addFd(fd, EPOLLIN | m_clntEvent);
    setnoblock(fd);
#ifdef DEBUG
        std::cout << "add client [" << fd << "] connection." << std::endl;
#endif 
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
    m_threadsPool->addTask(std::bind(&Webserver::onRead, this, client));
}

void Webserver::dealWrite(HttpConn* client)
{
    assert(client);
    extentTime(client);
    // 线程池添加任务
    m_threadsPool->addTask(std::bind(&Webserver::onWrite, this, client));
}

void Webserver::extentTime(HttpConn* client)
{
    assert(client);
    if(m_timeout > 0)
    {
        // 调整时间
        m_timer->adjust(client->getFd(), m_timeout);
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
#ifdef DEBUG
        std::cout << "Port:" << m_port << "error!" << std::endl;
#endif
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
#ifdef DEBUG
        std::cout << "Create socket error!" << std::endl;

#endif
        return false;
    }

    ret = setsockopt(m_listenFd, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0)
    {
        close(m_listenFd);
#ifdef DEBUG
        std::cout << "Init linger error!" << std::endl;
#endif       
        return false;
    }

    ret = bind(m_listenFd, (struct sockaddr*)&addr, sizeof(addr));
    if(ret < 0)
    {
        close(m_listenFd);
#ifdef DEBUG
        std::cout << "bind error!" << std::endl;
#endif 
        return false;
    }

    ret = listen(m_listenFd, 6);
    if(ret < 0)
    {
        close(m_listenFd);
#ifdef DEBUG
        std::cout << "listen error!" << std::endl;
#endif 
        return false;
    }

    ret = m_epoller->addFd(m_listenFd, m_listenEvent | EPOLLIN);
    if(ret == 0)
    {
        close(m_listenFd);
#ifdef DEBUG
        std::cout << "addFd error!" << std::endl;
#endif 
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