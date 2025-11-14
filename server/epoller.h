#ifndef EPOLLER_H
#define EPOLLER_H


#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <cerrno>

class Epoller
{



private:
    int epollfd;
    std::vector<struct epoll_event> m_events;

};

#endif