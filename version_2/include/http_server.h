#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

#include "http_peer.h"
#include "http_conn.h"
#include "threadpool.h"

#include <unistd.h>
#include <stdexcept>
#include <sys/epoll.h>

const int MAX_EPOLL_EVENTS = 1000;
const int MAX_USERS_COUNT = 1000;

class HttpServer
{
public:
    explicit HttpServer(char *ip, int port = 9090, int thread_num = 1000, int backlog = 5);
    ~HttpServer();

public:
    void run();

private:
    HttpPeerServer m_server;

    bool m_stop;
    threadpool<http_conn> *pool;
    int m_epfd;
    
    epoll_event events[MAX_EPOLL_EVENTS];
    http_conn users[MAX_USERS_COUNT];
};

#endif // HTTP_SERVER_H_