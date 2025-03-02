#ifndef HTTP_DEFINE_H_
#define HTTP_DEFINE_H_

// http请求方法
enum HTTP_METHOD
{
    METHOD_NONE,
    METHOD_GET,
    METHOD_HEAD,
    METHOD_POST,
    METHOD_PUT
};

// http版本
enum HTTP_VERSION
{
    HTTP_10,
    HTTP_11,
    HTTP_NOT_SUPPORT
};

// 请求报文头部字段
enum HTTP_HEAD
{
    Host = 0,
    User_Agent,
    Connection,
    Accept_Encoding,
    Accept_Language,
    Accept,
    Cache_Control,
    Upgrade_Insecure_Requests
};

// 解析一行请求报文状态
enum LINE_STATE
{
    LINE_OK,
    LINE_BAD,
    LINE_NOT_ENOUGH
};

// 当前解析请求报文哪一部分
enum PARSE_STATE
{
    PARSING_REQUEST,
    PARSING_HEAD,
    PARSEING_CONTENT
};

// 请求结果
enum HTTP_CODE
{
    NO_REQUEST,
    BAD_REQUEST,
    GET_REQUEST,
    NO_RESOURCE,
    FORBIDDEN_REQUEST,
    WRONG_TARGET,
    UNKNOWEN_ERROR
};

const int READ_BUFFER_SIZE = 4096;
const int WRITE_BUFFER_SIZE = 1024;
const int MAX_CLIENT_USERS = 2048;
const int MAX_FILE_PATH = 256;

#endif