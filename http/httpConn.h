#ifndef HTTPCONN_H
#define HTTPCONN_H

#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <cstdio>
#include <cerrno>

#include "httpRequest.h"
#include "httpResponse.h"


class HttpConn
{

public:
    HttpConn();
    ~HttpConn();

    void init(int sockfd, const sockaddr_in& addr);

    ssize_t readFromClnt(int* saveError);
    ssize_t writeToClnt(int* saveError);

    void closeConn();
    int getFd() const;
    int getPost() const;
    const char* getIp() const;
    sockaddr_in getAddr() const;

    bool process();

    int toWriteBytes() const
    {
        return m_iov[0].iov_len + m_iov[1].iov_len;
    }

    bool isKeepAlive() const
    {
        return m_request.isKeepAlive();
    }

public:
    static const char* srcDir;              // 静态资源目录
    static std::atomic<int> userCount;      // 记录当前活跃连接数
    static bool isET;                       // 标识连接是否使用边缘触发


private:
    int m_fd;
    struct sockaddr_in m_addr;

    bool m_isClose;

    int m_iovCnt;
    struct iovec m_iov[2];

    Buffer m_readBuff;          // 读缓冲区
    Buffer m_writeBuff;         // 写缓冲区

    HttpRequest m_request;
    HttpResponse m_response;
};

#endif