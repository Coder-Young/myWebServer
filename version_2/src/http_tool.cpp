#include <string.h>
#include <stdarg.h>

#include "../include/http_conn.h"
#include "../include/http_tool.h"

#define LF      '\r'
#define CR      '\n'
#define ED      '\0'
#define SP      " "

LINE_STATE http_parser::parse_line(char *buf, int &read_index, int &check_index)
{
    for (; check_index < read_index; check_index++)
    {
        if (buf[check_index] == LF)
        {
            if (check_index + 1 == read_index)
            {
                return LINE_NOT_ENOUGH;
            }
            else if (buf[check_index + 1] == CR)
            {
                buf[check_index++] = ED;
                buf[check_index++] = ED;
                return LINE_OK;
            }

            return LINE_BAD;
        }
        else if (buf[check_index] == CR)
        {
            if (check_index <= 0)
            {
                return LINE_BAD;
            }
            else if (buf[check_index - 1] == LF)
            {
                buf[check_index - 1] = ED;
                buf[check_index++] = ED;
                return LINE_OK;
            }

            return LINE_BAD;
        }
    }

    return LINE_NOT_ENOUGH;
}

HTTP_CODE http_parser::parse_requestline(char *line, PARSE_STATE &state, http_request& request)
{
    // GET http://jsuacm.cn/ HTTP/1.1
    char *url = strpbrk(line, SP);
    if (!url)
    {
        return BAD_REQUEST;
    }

    *url++ = ED;

    char *method = line;
    if (strcasecmp(method, "GET") == 0)
    {
        request.m_method = METHOD_GET;
    }
    else if (strcasecmp(method, "PUT") == 0)
    {
        request.m_method = METHOD_PUT;
    }
    else if (strcasecmp(method, "POST") == 0)
    {
        request.m_method = METHOD_POST;
    }
    else
    {
        return BAD_REQUEST;
    }

    char *version = strpbrk(url, SP);
    if (!version)
    {
        return BAD_REQUEST;
    }

    *version++ = ED;
    if (strcasecmp(version, "HTTP/1.0") == 0)
    {
        request.m_version = HTTP_10;
    }
    else if(strcasecmp(version, "HTTP/1.1") == 0)
    {
        request.m_version = HTTP_11;
    }

    if (strncasecmp(url, "http://", 7) == 0)
    {
        url += 7;
        url = strchr(url, '/');
    }
    else if (strncasecmp(url, "/", 1) == 0)
    {
    }
    else
    {
        return BAD_REQUEST;
    }

    request.m_url = std::string(url);
    state = PARSING_HEAD;

    return GET_REQUEST;
}

HTTP_CODE http_parser::parse_head(char *line, PARSE_STATE &state, http_request& request)
{
    if (line[0] == '\0')
    {
        if (request.m_content_length > 0)
        {
            state = PARSEING_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    
    if (strncasecmp(line, "Host:", 5) == 0)
    {
        line += 5;
        line += strspn(line, SP);
        request.m_host = line;
    }
    else if (strncasecmp(line, "Connection:", 11) == 0)
    {
        line += 11;
        line += strspn(line, SP);
        if (strcasecmp(line, "keep-alive") == 0)
        {
            request.m_keep_conn = true;
        }
    }
    else if (strncasecmp(line, "Accept:", 7) == 0)
    {

    }
    else if (strncasecmp(line, "Content-Length:", 15) == 0)
    {
        line += 15;
        line += strspn(line, SP);
        request.m_content_length = atoi(line);
    }
    else
    {
        printf("unknowen header:%s\n", line);
    }

    return NO_REQUEST;
}

HTTP_CODE http_parser::parse_content(char *line, int read_index, int check_index, http_request& request)
{
    if (read_index > (check_index + request.m_content_length))
    {
        line[read_index] = ED;
        return GET_REQUEST;
    }

    return NO_REQUEST;
}


bool http_response::add_message(char *msg, int &write_index, const char *format, ...)
{
    va_list arg_list;
    va_start(arg_list, format);

    int len = vsnprintf(msg + write_index, WRITE_BUFFER_SIZE - write_index - 1, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - write_index - 1))
    {
        return false;
    }

    write_index += len;
    va_end(arg_list);
    return true;
}

bool http_response::add_status_line(char *msg, int &write_index, http_request& request, int status_code, const char *status_info)
{
    if (write_index >= WRITE_BUFFER_SIZE)
    {
        return false;
    }

    char *version = 0;
    if (request.m_version == HTTP_10)
    {
        version = (char*)"HTTP/1.0";
    }
    else if (request.m_version == HTTP_11)
    {
        version = (char*)"HTTP/1.1";
    }
    else
    {
        return false;
    }

    // HTTP/1.1 200 OK\r\n
    return add_message(msg, write_index, "%s %d %s\r\n", version, status_code, status_info);
}

bool http_response::add_header(char *msg, int &write_index, http_request& request, int len)
{
    return add_message(msg, write_index, "Content-Length:%d\r\n", len) && 
            add_message(msg, write_index, "Connection:%s\r\n", (request.m_keep_conn == true) ? "keep alive" : "close") &&
            add_message(msg, write_index, "%s", "\r\n");
}

bool http_response::add_content(char *msg, int &write_index, const char *info)
{
    return add_message(msg, write_index, info);
}