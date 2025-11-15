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
public:
    explicit Epoller(int maxEvent = 1024);

    ~Epoller();
    
    bool addFd(int fd, uint32_t events);
    
    bool modFd(int fd, uint32_t);
    bool removeFd(int fd);

    int wait(int timeout = -1);

    int getEventFd(size_t ) const;

    uint32_t getEvents(size_t) const;

private:
    int m_epollfd;
    std::vector<struct epoll_event> m_events;

};

#endif