#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<fcntl.h>
#include<stdlib.h>
#include<cassert>
#include<sys/epoll.h>
#include "./locker.h"
#include "./my_threadpool.h"
#include "./my_http_conn.h"

#define MAX_EPOLL_EVENTS    10000
#define MAX_CLIENT_CONN     65536

//static int sig_pipefd[2];

extern void addfd(int epfd, int fd, bool one_shot);


void addsig(int signal, void(handler)(int), bool restart = true)
{
    struct sigaction act;
    act.sa_handler = handler;
    sigfillset(&act.sa_mask);
    if (restart)
    {
        act.sa_flags |= SA_RESTART;
    }
    assert(sigaction(signal, &act, NULL) != -1);
}

/*
void sig_handler(int sig)
{
    int errno_save = errno;
    int msg = sig;
    send(sig_pipefd[1], (char*)&msg, 1, 0);
    errno = errno_save;
}
*/

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Usage error\n");
        return 0;
    }

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    struct sockaddr_in addr;
    memset(&addr, sizeof(addr), 0);
    inet_pton(AF_INET, argv[1], &addr.sin_addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[2]));

    int ret = bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    assert(ret >= 0);

    ret = listen(listenfd, 5);
    assert(ret >= 0);

    //ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    //assert(ret >= 0);

    int epfd = epoll_create(5);
    assert(epfd >= 0);

    epoll_event events[MAX_EPOLL_EVENTS];
    addfd(epfd, listenfd, false);
    //addfd(epfd, sig_pipefd[0], false);

    http_conn *users = new http_conn[MAX_CLIENT_CONN];
    assert(users);

    threadpool<http_conn> *pool = nullptr;
    try
    {
        pool = new threadpool<http_conn>;
    }
    catch(...)
    {
        return 1;
    }

    addsig(SIGPIPE, SIG_IGN);
    //addsig(SIGINT, sig_handler);

    http_conn::m_epfd = epfd;
    bool m_stop = false;
    while (!m_stop)
    {
        int num = epoll_wait(epfd, events, MAX_EPOLL_EVENTS, -1);
        if (num < 0 && errno != EAGAIN)
        {
            if (errno != EINTR)
            {
                printf("epoll_wait() error\n");
                break;
            }
        }

        for (int i = 0; i < num; i++)
        {
            int curfd = events[i].data.fd;
            if (curfd == listenfd)
            {
                struct sockaddr_in clnt_addr;
                socklen_t clnt_addr_sz;
                int clntfd = accept(listenfd, (struct sockaddr*)&clnt_addr, &clnt_addr_sz);
                if (clntfd < 0)
                {
                    printf("accept() error:%d\n", errno);
                    continue;
                }

                if (http_conn::m_users_count >= MAX_CLIENT_CONN)
                {
                    printf("client full\n");
                    continue;
                }

                users[clntfd].init(clntfd, clnt_addr);
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                users[curfd].close_conn();
            }
            /*else if (curfd == sig_pipefd[0] && events[i].events & EPOLLIN)
            {
                // 信号处理
                char signals[1024];
                int ret = recv(curfd, signals, sizeof(signals), 0);
                if (ret <= 0)
                {
                    continue;
                }
                else
                {
                    for (int j = 0; j < ret; j++)
                    {
                        int signal = (int)signals[j];
                        switch (signal)
                        {
                            case SIGINT:
                            {
                                m_stop = true;
                                break;
                            }
                        }
                    }
                }

            }
            */else if (events[i].events & EPOLLIN)
            {
                if (users[curfd].read())
                {
                    pool->append(users + curfd);
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                if (!users[curfd].write())
                {
                    users[curfd].close_conn();
                }
            }
            else
            {
            }
        }
    }

    close(listenfd);
    close(epfd);
    delete [] users;
    delete pool;
    return 0;
}