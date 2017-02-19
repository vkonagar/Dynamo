#ifndef HTTP_PROTO_H
#define HTTP_PROTO_H
#include "http_header.h"

#define RESOURCE_TYPE_CGI_BIN   1
#define RESOURCE_TYPE_HTML      2
#define RESOURCE_TYPE_GIF       3
#define RESOURCE_TYPE_TXT       4
#define RESOURCE_TYPE_JPG       5
#define RESOURCE_TYPE_UNKNOWN   6

#define HTTP_200                10
#define HTTP_404                11

int http_scan_header(int clientfd, http_header_t* header);
int http_write_response_header(int clientfd, int http_response_code);
int get_resource_type(char* url, char* resource_name);
#endif
