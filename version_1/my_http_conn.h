#ifndef MY_HTTPCONNECTION_H_
#define MY_HTTPCONNECTION_H_
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>

class http_conn
{
public:
    static const int FILENAME_MAX_LEN = 256;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;

    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATCH
    };

    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,    // 解析请求行
        CHECK_STATE_HEADR,              // 解析头部
        CHECK_STATE_CONTENT             // 解析内容
    };

    enum HTTP_CODE
    {
        NO_REQUEST,        // 请求不完整，需要继续读取客户数据
        GET_REQUEST,       // 得了一个完整的客户请求
        BAD_REQUEST,       // 客户请求有语法错误
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR
    };

    enum LINE_STATUS
    {
        LINE_OK,
        LINE_BAD,
        LINE_NOT_ENOUGH
    };

public:
    http_conn() {}
    ~http_conn() {}
public:
    void init(int fd, struct sockaddr_in &addr);
    void close_conn();
    void process();
    bool read();
    bool write();

private:
    void init();
    char *getline() { m_read_buf + m_start_line; }
    HTTP_CODE fsm_process_read();
    LINE_STATUS parse_line();
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_header(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();

    bool process_write(HTTP_CODE ret);
    bool add_response(const char *format, ...);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_len);
    bool add_content(const char *content);
    void unmap();

public:
    static int m_epfd;
    static int m_users_count;

private:
    char m_read_buf[READ_BUFFER_SIZE];
    char m_write_buf[WRITE_BUFFER_SIZE];

    int m_sockfd;
    struct sockaddr_in m_addr;

    int m_read_index;   // 已读入数据最后一个字节后一个位置
    int m_check_index;  // 已检查数据最后一个字节后一个位置
    int m_start_line;   // 正在解析行的起始位置
    int m_write_index;

    CHECK_STATE m_cur_check_state;
    METHOD m_method;

    char m_file_path[FILENAME_MAX_LEN];

    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_keep_link;
    char *m_file_address;

    struct stat m_file_stat;

    struct iovec m_iv[2];
    int m_iv_count;
};

#endif