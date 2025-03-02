#include <stdlib.h>
#include <string>

#include "../include/http_server.h"

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage Error: <command> <ip> <port>\n");
        exit(0);
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);

    HttpServer http_server(ip, port);
    http_server.run();
    return 0;
}