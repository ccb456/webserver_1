#ifndef BUFFER_H
#define BUFFER_H

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/uio.h>
#include <vector>
#include <cassert>
#include <atomic>

using std::string;
using std::size_t;


class Buffer
{

public:
    Buffer(int initSize = 1024);
    ~Buffer() = default;

    size_t writeableBytes() const;      // 容器中还可写入的字节数
    size_t readableBytes() const;       // 容器中还可读入的字节数
    size_t curReadIdx() const;          // 当前读索引的位置

    const char* readBegin() const;           // 可读的起始位置
    void ensureWriteable(size_t len);       // 确保可以写入len字节的数据，如不能则扩容。
    void hasWritten(size_t len);            // 已经写入的字节数

    /* 定义一系列对 读索引操作的 接口 */
    void advance(size_t len);               // 读索引前进len
    void advance(const char* end);          // 读索引前进到某个位置
    void clear();                           // 清空缓冲区
    string clearAndToStr();                 // 清空缓冲区并返回内容

    const char* writeBeginConst() const;    // 开始写的位置,常量
    char* writeBegin();                     // 开始写的位置


    /* 定义一系列 写操作 接口 */
    void insert(const string& str);
    void insert(const char* str, size_t len);
    void insert(const void* data, size_t len);
    void insert(const Buffer& buff);

    ssize_t readFromFd(int fd, int* errnoInfo);     // 从sockfd 读入
    ssize_t writeToFd(int fd, int* errnoInfo);    // 从sockfd 写入

private:
    char* _begin();                             // 获取首地址
    const char* _begin() const;

    void _makeSpace(size_t len);

private:

    /**
     *  可读区间 [readIdx, writeIdx)
     *  可写区间 [writeIdx, _buffer.size())
     */
    std::atomic<size_t> m_readIdx;              // 当前读的位置
    std::atomic<size_t> m_writeIdx;             // 当前写的位置

    std::vector<char> m_buffer;                 // 底层缓冲区

};

#endif