#ifndef UTIL_H_
#define UTIL_H_

#include <stdint.h>

int setnoneblock(int fd);

void addfd(int epfd, int fd, bool bOneShot = true);

void removefd(int epfd, int fd);

void modfd(int epfd, int fd, uint32_t listen_event);

#endif