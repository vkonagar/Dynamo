#ifndef HTTP_PROTO_H
#define HTTP_PROTO_H
#include "http_header.h"

int scan_header(int clientfd, http_header_t* header);

#endif
