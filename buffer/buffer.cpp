#include "buffer.h"

Buffer::Buffer(int initSize): m_buffer(initSize), m_readIdx(0), m_writeIdx(0)
{

}

size_t Buffer::writeableBytes() const
{
    return m_buffer.size() - m_writeIdx;
}

size_t Buffer::readableBytes() const
{
    return m_writeIdx - m_readIdx;
}

size_t Buffer::curReadIdx() const
{
    return m_readIdx;
}

const char* Buffer::readBegin() const
{
    return _begin() + m_readIdx;
}

void Buffer::ensureWriteable(size_t len)
{
    assert(len >= 0);

    if(writeableBytes() < len)
    {
        _makeSpace(len);
    }

    assert(writeableBytes() >= len);
}

void Buffer::hasWritten(size_t len)
{
    assert(len >= 0);
    m_writeIdx += len;
}

void Buffer::advance(size_t len)
{
    assert(len <= readableBytes());
    m_readIdx += len;
}

void Buffer::advance(const char* end)
{
    assert(end != nullptr && readBegin() <= end);
    advance(static_cast<size_t>(end - readBegin()));
}

void Buffer::clear()
{
    bzero(_begin(), m_buffer.size());
    m_readIdx = 0;
    m_writeIdx = 0;
}

string Buffer::clearAndToStr()
{
    string str(readBegin(), readableBytes());
    clear();
    return str;
}

const char* Buffer::writeBeginConst() const
{
    return _begin() + m_writeIdx;
}

char* Buffer::writeBegin()
{
    return _begin() + m_writeIdx;
}

void Buffer::insert(const string& str)
{
    insert(str.c_str(), str.length());
}

void Buffer::insert(const char* str, size_t len)
{
    assert(str);
    ensureWriteable(len);
    // 将新数据直接复制到缓冲区
    std::copy(str, str + len, writeBegin());
    hasWritten(len);
}

void Buffer::insert(const void* data, size_t len)
{
    insert(static_cast<const char*>(data), len);
}

void Buffer::insert(const Buffer& buff)
{
    insert(buff.readBegin(), buff.readableBytes());
}

ssize_t Buffer::readFromFd(int fd, int* errnoInfo)
{
    char buff[65535];
    struct iovec iov[2];

    const size_t writeCnt = writeableBytes();

    /* 分散读，保证数据全部都可以读完 */
    iov[0].iov_base = _begin() + m_writeIdx;
    iov[0].iov_len = writeCnt;

    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2);
    if(len < 0)
    {
        *errnoInfo = errno;
    }
    else if(static_cast<size_t>(len) <= writeCnt)
    {
        m_writeIdx += len;
    }
    else
    {
        m_writeIdx = m_buffer.size();
        insert(buff, len - writeCnt);
    }


    return len;
}

ssize_t Buffer::writeToFd(int fd, int* errnoInfo)
{
    size_t readSize = readableBytes();
    ssize_t len = write(fd, readBegin(), readSize);

    if(len < 0)
    {
        *errnoInfo = errno;

    }
    else
    {
        m_readIdx += len;
    }

    return len;
}

char* Buffer::_begin()
{
    return &*m_buffer.begin();
}

const char* Buffer::_begin() const
{
    return &*m_buffer.begin();
}

void Buffer::_makeSpace(size_t len) 
{
    size_t available = writeableBytes() + curReadIdx();         // 总可用空间
    size_t readable = readableBytes();                         // 提前记录当前可读数据长度

    // 若空间不足，先扩容至足够容纳「原有数据和总可用容量之间的最大值 + 新需求数据」
    if (available < len) 
    {
        size_t tmp = std::max(available, readable);
        m_buffer.resize(tmp + len);  
    }

    // 无论是否扩容，都将数据迁移到容器开头，并重置读写指针
    std::copy(_begin() + m_readIdx, _begin() + m_writeIdx, _begin());
    m_readIdx = 0;
    m_writeIdx = readable;
    assert(readableBytes() == readable);  // 验证数据完整性
}
