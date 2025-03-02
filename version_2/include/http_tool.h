#ifndef HTTP_TOOL_H_
#define HTTP_TOOL_H_

#include "http_define.h"

struct http_request;
class http_parser
{
public:
    static LINE_STATE parse_line(char *buf, int &read_index, int &check_index);

    static HTTP_CODE parse_requestline(char *line, PARSE_STATE &state, http_request& request);

    static HTTP_CODE parse_head(char *line, PARSE_STATE &state, http_request& request);

    static HTTP_CODE parse_content(char *line, int read_index, int check_index, http_request& request);
};

class http_response
{
public:
    
    static bool add_status_line(char *msg, int &write_index, 
            http_request& request, int status_code, const char *status_info);

    static bool add_header(char *msg, int &write_index, http_request& request, int len);

    static bool add_content(char *msg, int &write_index, const char *info);

private:

    static bool add_message(char *msg, int &write_index, const char *format, ...);
};

#endif