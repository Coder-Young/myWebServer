#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#include "../include/util.h"
#include "../include/http_conn.h"

const char *base_root = "/var/www/html";
int http_conn::m_user_count = 0;

http_conn::http_conn()
{

}

http_conn::~http_conn()
{

}

void http_conn::init(int epfd, int fd, struct sockaddr_in addr, socklen_t len)
{
    m_epfd = epfd;
    m_fd = fd;
    m_addr = addr;
    m_addr_sz = len;

    m_user_count += 1;
    addfd(m_epfd, m_fd);
    printf("Success connect Client[%d]\n", m_fd);

    init();
}

void http_conn::init()
{
    m_read_index = 0;
    m_check_index = 0;
    m_write_index = 0;
    m_parse_start_line = 0;
    m_file_address = 0;
    m_iv_count = 0;

    memset(m_read_buf, 0, sizeof(m_read_buf));
    memset(m_write_buf, 0, sizeof(m_write_buf));
    memset(m_file_path, 0, sizeof(m_file_path));

    printf("Client[%d] Success init\n", m_fd);
}

void http_conn::close_conn()
{
    removefd(m_epfd, m_fd);
    close(m_fd);
    m_user_count -= 1;
    printf("Success close Client[%d]\n", m_fd);
}

bool http_conn::read()
{
    if (m_read_index >= READ_BUFFER_SIZE)
    {
        return false;
    }

    while (true)
    {
        int bytes_get = recv(m_fd, m_read_buf + m_read_index, READ_BUFFER_SIZE - m_read_index - 1, 0);
        if (bytes_get < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            return false;
        }
        else if (bytes_get == 0)
        {
            return false;
        }

        m_read_index += bytes_get;
    }

    printf("Server Success read %d bytes from Client[%d]\n", m_read_index, m_fd);
    return true;
}

bool http_conn::write()
{
    int bytes_to_send = m_write_index;
    int bytes_have_send = 0;
    if (bytes_to_send <= 0)
    {
        modfd(m_epfd, m_fd, EPOLLIN);
        init();
        return true;
    }

    while (1)
    {
        int sended = writev(m_fd, m_iv, m_iv_count);
        if (sended <= -1)
        {
            if (errno == EAGAIN)
            {
                // 写缓冲区满，重新等
                modfd(m_epfd, m_fd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += sended;
        bytes_to_send -= sended;
        if (bytes_have_send >= m_write_index)
        {
            unmap();
            if (m_request.m_keep_conn)
            {
                modfd(m_epfd, m_fd, EPOLLIN);
                init();
                return true;
            }
            else
            {
                modfd(m_epfd, m_fd, EPOLLIN);
                return false;
            }
        }
    }
}

void http_conn::process()
{
    HTTP_CODE ret = process_request_message();
    if (ret == BAD_REQUEST)
    {
        modfd(m_epfd, m_fd, EPOLLIN);
        return;
    }

    ret = get_request_file();
    if (!process_resonse_message(ret))
    {
        close_conn();
    }

    modfd(m_epfd, m_fd, EPOLLOUT);
    return;
}

HTTP_CODE http_conn::process_request_message()
{
    m_request.init();
    PARSE_STATE curState = PARSING_REQUEST;
    LINE_STATE code = LINE_BAD;
    while ((code = http_parser::parse_line(m_read_buf, m_read_index, m_check_index)) == LINE_OK)
    {
        char *sub_line = m_read_buf + m_parse_start_line;
        m_parse_start_line = m_check_index;

        switch(curState)
        {
            case PARSING_REQUEST:
            {
                HTTP_CODE ret = http_parser::parse_requestline(sub_line, curState, m_request);
                if (ret == BAD_REQUEST)
                {
                    return ret;
                }
                break;
            }
            case PARSING_HEAD:
            {
                HTTP_CODE ret = http_parser::parse_head(sub_line, curState, m_request);
                if (ret != NO_REQUEST)
                {
                    return ret;
                }
                break;
            }
            case PARSEING_CONTENT:
            {
                HTTP_CODE ret = http_parser::parse_content(sub_line, m_read_index, m_check_index, m_request);
                if (ret == GET_REQUEST)
                {
                    return ret;
                }
                break;
            }
            default:
            {
                break;
            }
        }
    }

    return BAD_REQUEST;
}

HTTP_CODE http_conn::get_request_file()
{
    strcpy(m_file_path, base_root);
    int len = strlen(base_root);
    strncpy(m_file_path + len, m_request.m_url.c_str(), MAX_FILE_PATH - len - 1);

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
        return WRONG_TARGET;
    }

    int fd = open(m_file_path, O_RDONLY);
    if (fd < 0)
    {
        return UNKNOWEN_ERROR;
    }

    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    ::close(fd);
    return GET_REQUEST;
}

bool http_conn::process_resonse_message(HTTP_CODE ret)
{
    switch(ret)
    {
        case GET_REQUEST:
        {
            http_response::add_status_line(m_write_buf, m_write_index, m_request, 200, "OK");
            if (m_file_stat.st_size > 0)
            {
                http_response::add_header(m_write_buf, m_write_index, m_request, m_file_stat.st_size);
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
                http_response::add_header(m_write_buf, m_write_index, m_request, strlen(ok_string));
                http_response::add_content(m_write_buf, m_write_index, ok_string);
            }
            break;
        }
        case NO_RESOURCE:
        {
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            break;
        }
        case WRONG_TARGET:
        {
            break;
        }
        case UNKNOWEN_ERROR:
        {
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

void http_conn::unmap()
{
    if (m_file_address != NULL)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}