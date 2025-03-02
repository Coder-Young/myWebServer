#include "../include/http_server.h"
#include "../include/util.h"

HttpServer::HttpServer(char *ip, int port/*= 9090*/, int thread_num/*= 500*/, int backlog/*= 5*/) : m_server(HttpPeerServer(ip, port)), m_stop(false)
{
    if (m_server.bind() < 0)
    {
        throw std::runtime_error("bind() error");
    }

    if (m_server.listen(backlog) < 0)
    {
        throw std::runtime_error("listen() error");
    }

    pool = new threadpool<http_conn>(thread_num);
    if (pool == NULL)
    {
       throw std::runtime_error("epoll_create() error");
    }

    m_epfd = epoll_create(backlog);
    if (m_epfd < 0)
    {
        throw std::runtime_error("epoll_create() error");
    }

    addfd(m_epfd, m_server.m_listenfd, false);
}

HttpServer::~HttpServer()
{
    if (m_epfd != -1)
    {
        ::close(m_epfd);
    }
}

void HttpServer::run()
{
    while (!m_stop)
    {
        int ret = epoll_wait(m_epfd, events, MAX_EPOLL_EVENTS, -1);
        for (int i = 0; i < ret; i++)
        {
            int curfd = events[i].data.fd;
            if (curfd == m_server.m_listenfd)
            {
                // 接受客户端连接
                int clntfd;
                struct sockaddr_in clntaddr;
                socklen_t clntaddr_sz;
                m_server.accept(clntfd, &clntaddr, &clntaddr_sz);
                if (http_conn::m_user_count >= MAX_USERS_COUNT)
                {
                    char msg[] = "client full";
                    send(clntfd, msg, sizeof(msg), 0);
                    continue;
                }

                users[clntfd].init(m_epfd, clntfd, clntaddr, clntaddr_sz);
            }
            else if (events[i].events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR))
            {
                // 对端关闭
                users[curfd].close_conn();
            }
            else if (events[i].events & EPOLLIN)
            {
                // 对端数据
                if (users[curfd].read())
                {
                    pool->append(&users[curfd]);
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                // 有数据发送
                if (!users[curfd].write())
                {
                    users[curfd].close_conn();
                }
            }
            else
            {}
        }
    }
}