#include "../include/http_peer.h"

#include <iostream>
#include <stdexcept>
#include <string.h>
#include <unistd.h>

HttpPeerServer::HttpPeerServer()
{
    
}

HttpPeerServer::HttpPeerServer(char *ip, int port/*= 9090*/) :
                            m_ip(ip), m_port(port)
{
    m_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenfd == -1)
    {
        throw std::runtime_error("socket() error");
    }

    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = PF_INET;
    m_addr.sin_port = htons(m_port);
    if (m_ip)
    {
        inet_pton(AF_INET, m_ip, &m_addr.sin_addr);
    }
    else
    {
        m_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
}

HttpPeerServer::~HttpPeerServer()
{
    ::close(m_listenfd);
}

int HttpPeerServer::bind()
{
    return ::bind(m_listenfd, (struct sockaddr*)&m_addr, sizeof(m_addr));
}

int HttpPeerServer::listen(int backlog)
{
    return ::listen(m_listenfd, backlog);
}

bool HttpPeerServer::accept(int &fd, struct sockaddr_in *clntaddr, socklen_t *clntaddr_sz)
{
    fd = ::accept(m_listenfd, (struct sockaddr*)clntaddr, clntaddr_sz);
    if (fd < 0)
    {
        std::cout << "accept error in file <" << __FILE__ << "> "
                << "at " << __LINE__ << std::endl;
        exit(0);
    }

    //std::cout << "[CLIENT]" << fd << " connected" << std::endl;
    return true;
}