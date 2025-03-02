#include "../include/util.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

int setnoneblock(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epfd, int fd, bool bOneShot/*= true*/)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLET | EPOLLIN | EPOLLHUP;
    if (bOneShot)
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
    setnoneblock(fd);
}

void removefd(int epfd, int fd)
{
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
}

void modfd(int epfd, int fd, uint32_t listen_event)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = listen_event | EPOLLET | EPOLLRDHUP | EPOLLONESHOT;
    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event);
}