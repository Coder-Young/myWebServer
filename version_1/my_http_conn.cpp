#include "my_http_conn.h"

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file from this server.\n ";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n ";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the requested file.\n ";

const char *doc_root = "/var/www/html";

int setnoneblock(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLET | EPOLLIN | EPOLLRDHUP;
    if (one_shot)
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
    setnoneblock(fd);
}

void modfd(int epfd, int fd, uint32_t events)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = events | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event);
}

void removefd(int epfd, int fd)
{
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
}

int http_conn::m_users_count = 0;
int http_conn::m_epfd = -1;

void http_conn::init(int fd, struct sockaddr_in &addr)
{
    m_sockfd = fd;
    m_addr = addr;

    addfd(m_epfd, m_sockfd, true);
    m_users_count++;
    init();
}

void http_conn::init()
{
    m_cur_check_state = CHECK_STATE_REQUESTLINE;
    m_keep_link = false;
    m_method = GET;

    char *m_url = 0;
    char *m_version = 0;
    char *m_host = 0;
    int m_content_length = 0;
    char *m_file_address = 0;

    int m_read_index = 0;
    int m_check_index = 0;
    int m_start_line = 0;
    int m_write_index = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_file_path, '\0', FILENAME_MAX_LEN);
}

void http_conn::close_conn()
{
    removefd(m_epfd, m_sockfd);
    m_sockfd = -1;
    m_users_count--;
}

bool http_conn::read()
{
    if (m_read_index >= READ_BUFFER_SIZE)
    {
        return false;
    }

    while (true)
    {
        int bytes_read = recv(m_sockfd, m_read_buf + m_read_index, READ_BUFFER_SIZE - m_read_index, 0);
        if (bytes_read < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            return false;
        }
        else if (bytes_read == 0)
        {
            return false;
        }

        m_read_index += bytes_read;
    }

    return true;
}

void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool http_conn::write()
{
    int bytes_to_send = m_write_index;
    int bytes_have_send = 0;
    if (bytes_to_send == 0)
    {
        modfd(m_epfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while (1)
    {
        int tmp = writev(m_sockfd, m_iv, m_iv_count);
        if (tmp <= -1)
        {
            if (errno == EAGAIN)
            {
                // 发送缓冲区满了，继续等待EPOLLOUT
                modfd(m_epfd, m_sockfd, EPOLLOUT);
                return true;
            }

            unmap();
            return false;
        }

        bytes_to_send -= tmp;
        bytes_have_send += tmp;
        if (bytes_have_send >= bytes_to_send)
        {
            unmap();
            if (m_keep_link)
            {
                init();
                modfd(m_epfd, m_sockfd, EPOLLIN);
                return true;
            }
            else
            {
                // 不直接返回false，继续监听以防残余数据
                modfd(m_epfd, m_sockfd, EPOLLIN);
                return false;
            }
        }
    }
}

void http_conn::process()
{
    HTTP_CODE ret = fsm_process_read();
    if (ret == BAD_REQUEST)
    {
        modfd(m_epfd, m_sockfd, EPOLLIN);
        return;
    }

    bool write_ret = process_write(ret);
    if (!write_ret)
    {
        close_conn();
    }

    modfd(m_epfd, m_sockfd, EPOLLOUT);
}

bool http_conn::add_response(const char *format, ...)
{
    if (m_write_index >= WRITE_BUFFER_SIZE)
    {
        return false;
    }

    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_index, WRITE_BUFFER_SIZE - m_write_index - 1, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - m_write_index - 1))
    {
        return false;
    }

    m_write_index += len;
    va_end(arg_list);
    return true;
}

bool http_conn::add_status_line(int status, const char *title)
{
    // HTTP/1.1 200 OK\r\n
    add_response("%s%d%s\r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_len)
{
    add_response("Content-Length:%d\r\n", content_len);
    add_response("Connection:%s\r\n",(m_keep_link==true)?"keep alive":"close");
    add_response("%s", "\r\n");
}

bool http_conn::add_content(const char *content)
{
    add_response("%s", content);
}

bool http_conn::process_write(HTTP_CODE ret)
{
    switch(ret)
    {
        case BAD_REQUEST:
        {
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            add_content(error_400_form);
            break;
        }
        case NO_RESOURCE:
        {
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            add_content(error_404_form);
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            add_content(error_403_form);
            break;
        }
        case INTERNAL_ERROR:
        {
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            add_content(error_500_form);
            break;
        }
        case FILE_REQUEST:
        {
            add_status_line(200, ok_200_title);
            if (m_file_stat.st_size != 0)
            {
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_index;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                return true;
            }
            else
            {
                const char *ok_string = "<html><body></body></html>";
                add_headers(strlen(ok_string));
                add_content(ok_string);
            }
            break;
        }
        default:
        {
            return false;
        }
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_index;
    m_iv_count = 1;
    return true;
}

// 状态机
http_conn::HTTP_CODE http_conn::fsm_process_read()
{
    char *text = 0;
    HTTP_CODE ret = NO_REQUEST;
    LINE_STATUS line_statue = LINE_OK;
    while ((m_cur_check_state == CHECK_STATE_CONTENT && line_statue == LINE_OK) || 
        (line_statue = parse_line()) == LINE_OK)
    {
        text = m_read_buf + m_start_line;
        m_start_line = m_check_index;

        switch (m_cur_check_state)
        {
            case CHECK_STATE_REQUESTLINE:
            {
                ret = parse_request_line(text);
                if (ret == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADR:
            {
                ret = parse_header(text);
                if (ret == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                else if (ret == GET_REQUEST)
                {
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT:
            {
                ret = parse_content(text);
                if (ret == GET_REQUEST)
                {
                    return do_request();
                }

                line_statue = LINE_NOT_ENOUGH;
                break;
            }
            default:
            {
                return INTERNAL_ERROR;
            }
        }
    }
}

http_conn::LINE_STATUS http_conn::parse_line()
{
    for (; m_check_index < m_read_index; m_check_index++)
    {
        char tmp = m_read_buf[m_check_index];
        if (m_check_index + 1 >= m_read_index)
        {
            return LINE_NOT_ENOUGH;
        }
        else if (tmp == '\r' && m_read_buf[m_check_index + 1] == '\n')
        {
            m_read_buf[m_check_index++] = '\0';
            m_read_buf[m_check_index++] = '\0';
            return LINE_OK;
        }
        else if (tmp == '\n')
        {
            if (m_check_index > 0 && m_read_buf[m_check_index - 1] == '\r')
            {
                m_read_buf[m_check_index - 1] = '\0';
                m_read_buf[m_check_index++] = '\0';
                return LINE_OK;
            }
            return LINE_NOT_ENOUGH;
        }
    }

    return LINE_NOT_ENOUGH;
}

http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    // GET /index.html HTTP/1.1\r\n
    m_url = strpbrk(text, " ");
    if (!m_url)
    {
        return BAD_REQUEST;
    }

    *m_url++ = '\0';
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
    {
        m_method = GET;
    }
    else
    {
        return BAD_REQUEST;
    }

    m_url += strspn(m_url, " ");
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if (!m_url || m_url[0] != '/')
    {
        return BAD_REQUEST;
    }

    m_version = strpbrk(m_url, " ");
    if (!m_version)
    {
        return BAD_REQUEST;
    }

    *m_version++ = '\0';
    m_version += strspn(m_version, " ");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
    {
        return BAD_REQUEST;
    }

    m_cur_check_state = CHECK_STATE_HEADR;
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_header(char *text)
{
    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {
            m_cur_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        // Host: www.example.com\0\0
        text += 5;
        text += strspn(text, " ");
        m_host = text;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        // Connection: keep-alive\0\0
        text += 11;
        text += strspn(text, " ");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_keep_link = true;
        }
    }
    else if (strncasecmp(text, "Accept:", 7) == 0)
    {
        // Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\0\0
    }
    else if (strncasecmp(text, "Content-Length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " ");
        m_content_length = atoi(text);
    }
    else
    {
        printf("unknow header\n");
    }

    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    if (m_read_index >= (m_check_index + m_content_length))
    {
        text[m_read_index] = '\0';
        return GET_REQUEST;
    }

    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(m_file_path, doc_root);
    int len = strlen(doc_root);
    strncpy(m_file_path + len, m_url, FILENAME_MAX - len - 1);

    if (stat(m_file_path, &m_file_stat) < 0)
    {
        return NO_RESOURCE;
    }

    if (!(m_file_stat.st_mode & S_IROTH))
    {
        return FORBIDDEN_REQUEST;
    }

    if (S_ISDIR(m_file_stat.st_mode))
    {
        return BAD_REQUEST;
    }

    int fd = open(m_file_path, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    ::close(fd);
    return GET_REQUEST;
}