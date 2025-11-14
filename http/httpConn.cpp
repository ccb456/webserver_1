#include "httpConn.h"

const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount = 0;
bool HttpConn::isET;

HttpConn::HttpConn()
:m_fd(-1), m_iovCnt(2), m_isClose(false)
{
    m_addr = { 0 };
}

HttpConn::~HttpConn()
{
    closeConn();
}

void HttpConn::init(int fd, const sockaddr_in& addr)
{
    assert(fd > 0);
    ++userCount;
    m_fd = fd;
    m_addr = addr;

    m_readBuff.clear();
    m_writeBuff.clear();
    m_isClose = false;

}

void HttpConn::closeConn()
{
    m_response.unmap();
    if(m_isClose == false)
    {
        m_isClose = true;
        --userCount;
        close(m_fd);
    }
}

int HttpConn::getFd() const
{
    return m_fd;
}

sockaddr_in HttpConn::getAddr() const
{
    return m_addr;
}

int HttpConn::getPost() const
{
    return m_addr.sin_port;
}

const char* HttpConn::getIp() const
{
    return inet_ntoa(m_addr.sin_addr);
}

ssize_t HttpConn::readFromClnt(int* saveErrno)
{
    ssize_t len = -1;
    do
    {
        len = m_readBuff.readFromFd(m_fd, saveErrno);
        if(len < 0)
        {
            break;
        }
        
    } while (isET);

    return len;
}

ssize_t HttpConn::writeToClnt(int* saveError)
{
    ssize_t len = 0;
    do
    {
        len = writev(m_fd, m_iov, m_iovCnt);
        if(len <= 0)
        {
            *saveError = errno;
            break;
        }

        // 传输结束
        if(m_iov[0].iov_len + m_iov[1].iov_len == 0) break;
        else if(static_cast<size_t>(len) > m_iov[0].iov_len)
        {
            // 响应头已经写完，更新文件数据的指针和长度
            m_iov[1].iov_base = (uint8_t*)m_iov[1].iov_base + (len - m_iov[0].iov_len);
            m_iov[1].iov_len -= (len - m_iov[0].iov_len);
            
            if(m_iov[0].iov_len)
            {
                m_iov[0].iov_len = 0;
                m_writeBuff.clear();
            }
        }
        else
        {
            // 响应头未写完，更新响应头
            m_iov[0].iov_base = (uint8_t*)m_iov[0].iov_base + len;
            m_iov[1].iov_base -= len;
            m_writeBuff.advance(len);
        }
        
    } while (isET || toWriteBytes() > 10240);      // ET模式或大数据量时循环写

    return len;
    
}

bool HttpConn::process()
{
    m_request.init();
    if(m_readBuff.readableBytes() <= 0)
    {
        return false;
    }
    
    if(m_request.parse(m_readBuff))
    {
        // 解析成功
        // 初始化响应：资源目录、请求路径、长连接标志、状态码200
        m_response.init(srcDir, m_request.path(), m_request.isKeepAlive(), 200);
    }
    else
    {
        // 解析失败
        m_response.init(srcDir, m_request.path(), false, 400);
    }

    // 生成响应写到缓冲区
    m_response.makeResponse(m_writeBuff);

    /* 响应头 */
    m_iov[0].iov_base = const_cast<char*>(m_writeBuff.readBegin());
    m_iov[0].iov_len = m_writeBuff.readableBytes();
    m_iovCnt = 1;

    /* 文件 */
    if(m_response.fileLen() > 0 && m_response.fileAddr())
    {
        m_iov[1].iov_base = m_response.fileAddr();
        m_iov[1].iov_len = m_response.fileLen();
        m_iovCnt = 2;
    }

    return true;
}