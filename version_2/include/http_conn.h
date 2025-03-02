#ifndef HTTP_CONN_H_
#define HTTP_CONN_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>

#include "http_tool.h"
#include "http_define.h"

struct http_request
{

    void init()
    {
        m_method = METHOD_NONE;
        m_version = HTTP_NOT_SUPPORT;
        m_url = "";
        m_content = 0;
        m_keep_conn = false;
        m_content_length = 0;
        m_host = 0;
    }

    HTTP_METHOD m_method;
    HTTP_VERSION m_version;

    std::string m_url;
    char *m_content;

    // 头部字段
    bool m_keep_conn;
    int m_content_length;
    char *m_host;
};

class http_conn
{
public:
    http_conn();
    ~http_conn();

public:
    void init(int epfd, int fd, struct sockaddr_in addr, socklen_t len);
    void init();

    void close_conn();

    bool read();
    bool write();
    
    void process();
    HTTP_CODE process_request_message();
    HTTP_CODE get_request_file();
    bool process_resonse_message(HTTP_CODE ret);

private:
    void unmap();

public:
    static int m_user_count;

private:
    char m_read_buf[READ_BUFFER_SIZE];
    char m_write_buf[WRITE_BUFFER_SIZE];

    int m_epfd;
    int m_fd;
    struct sockaddr_in m_addr;
    socklen_t m_addr_sz;

    int m_read_index;
    int m_check_index;
    int m_write_index;
    int m_parse_start_line;

    char m_file_path[MAX_FILE_PATH];
    struct stat m_file_stat;
    char *m_file_address;

    http_request m_request;
    http_response m_response;

    struct iovec m_iv[2];
    int m_iv_count;
};



#endif