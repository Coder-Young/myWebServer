#ifndef HTTP_PEER_H_
#define HTTP_PEER_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>

class HttpPeerServer
{
public:
    HttpPeerServer();
    explicit HttpPeerServer(char *ip, int port = 9090);
    ~HttpPeerServer();

    int bind();

    int listen(int backlog);

    bool accept(int &fd, struct sockaddr_in *clntaddr, socklen_t *clntaddr_sz);

public:
    int m_listenfd;
    char *m_ip;
    int m_port;
    sockaddr_in m_addr;
};

#endif