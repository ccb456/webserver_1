#include "epoller.h"

Epoller::Epoller(int maxEvents)
:m_epollfd(epoll_create(512)), m_events(maxEvents)
{
    assert(m_epollfd >= 0 && m_events.size() > 0);
}

Epoller::~Epoller()
{
    close(m_epollfd);
}

bool Epoller::addFd(int fd, uint32_t events)
{
    if(fd < 0) return false;

    epoll_event ev = { 0 };
    ev.data.fd = fd;
    ev.events = events;

    return 0 == epoll_ctl(m_epollfd, EPOLL_CTL_ADD, fd, &ev);
}

bool Epoller::modFd(int fd, uint32_t events)
{
    if(fd < 0) return false;
    epoll_event ev = { 0 };
    ev.data.fd = fd;
    ev.events = events;

    return 0 == epoll_ctl(m_epollfd, EPOLL_CTL_MOD, fd, &ev);

}

bool Epoller::removeFd(int fd)
{
    if(fd < 0) return false;

    return 0 == epoll_ctl(m_epollfd, EPOLL_CTL_DEL, fd, nullptr);
}

int Epoller::wait(int timeout)
{
    return epoll_wait(m_epollfd, &m_events[0], static_cast<int>(m_events.size()), timeout);
}

int Epoller::getEventFd(size_t idx) const
{
    assert(idx >= 0 && idx < m_events.size());
    return m_events[idx].data.fd;
}

uint32_t Epoller::getEvents(size_t idx) const
{
    assert(idx >= 0 && idx < m_events.size());
    return m_events[idx].events;
}