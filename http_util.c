#include "http_util.h"
#include "csapp.h"
#include <stdbool.h>

int get_resource_type(char* url, char* resource_name)
{
    if (sscanf(url, "/cgi-bin/%s", resource_name) == 1)
    {
        return RESOURCE_TYPE_CGI_BIN;
    }
    else if (sscanf(url, "/%s.html", resource_name) == 1)
    {
        return RESOURCE_TYPE_HTML;
    }
    else if (sscanf(url, "/%s.txt", resource_name) == 1)
    {
        return RESOURCE_TYPE_TXT;
    }
    else if (sscanf(url, "/%s.gif", resource_name) == 1)
    {
        return RESOURCE_TYPE_GIF;
    }
    else if (sscanf(url, "/%s.jpg", resource_name) == 1)
    {
        return RESOURCE_TYPE_JPG;
    }
    return RESOURCE_TYPE_UNKNOWN;
}

int http_write_response_header(int clientfd, int http_response_code)
{
    char* response_str;
    switch (http_response_code)
    {
        case HTTP_200: response_str = "HTTP/1.0 200 OK\r\n";
                       break;
        case HTTP_404: response_str = "HTTP/1.0 404 Not Found\r\n";
                       break;
    }
    write(clientfd, response_str, strlen(response_str));
    write(clientfd, "\r\n", 2);
}

int http_scan_header(int clientfd, http_header_t* header)
{
    rio_t rio; /* For robust IO */
    bool request_format_scanned = false; /* Indicates if the first line of
                                        HTTP request is scanned succesfully */
    bool other_headers_scanned = false; /* Indicates if the other headers of
                                        HTTP request are scanned succesfully*/
    char temp_buffer[MAX_READLINE_STR_LENGTH];
    Rio_readinitb(&rio, clientfd);

    /* Scan the request header */
    if (rio_readlineb(&rio, temp_buffer, MAX_READLINE_STR_LENGTH) > 0)
    {
        if (sscanf(temp_buffer, STR_FMTB(MAX_REQUEST_TYPE_LENGTH)" "
                                STR_FMTB(MAX_URL_LENGTH)" "
                                STR_FMTB(MAX_HTTP_VERSION_LENGTH),
                    header->request_type,
                    header->request_url,
                    header->request_http_version) != 3)
        {
            return HTTP_INVALID_REQUEST;
        }

        /* Error checking.
         * Check the request type. Only GET is supported */
        if (strcmp(header->request_type, "GET") != 0)
        {
            return HTTP_REQ_TYPE_NOT_SUPPORTED;
        }

        /* Check the HTTP Version, only 1.1 or 1.0 is supported */
        if ( ! ((strcmp(header->request_http_version, "HTTP/1.0") == 0)
             || strcmp(header->request_http_version, "HTTP/1.1") == 0))
        {
            return HTTP_VERSION_NOT_SUPPORTED;
        }
        request_format_scanned = true;
    }

    /* Scan the headers */
    while (rio_readlineb(&rio, temp_buffer, MAX_READLINE_STR_LENGTH) > 0)
    {
        header_kv_pair_t* hdr = (header_kv_pair_t*)
                                Malloc(sizeof (header_kv_pair_t));
        /* Assumption is that key and values don't exceed 200 chars */
        int ret = sscanf(   temp_buffer,
                            STR_FMTB(MAX_HEADER_VALUE_LENGTH)
                            " "STR_FMTB(MAX_HEADER_VALUE_LENGTH),
                            hdr->key, hdr->value);
        if (ret == 2)
        {
            if (strcmp(hdr->key, "Host:")==0)
            {
                strncpy(header->host, hdr->value, MAX_HEADER_VALUE_LENGTH);
            }
            else if (strcmp(hdr->key, "User-Agent:")==0)
            {
                strncpy(header->user_agent, hdr->value, MAX_HEADER_VALUE_LENGTH);
            }
            else if (strcmp(hdr->key, "Connection:")==0)
            {
                strncpy(header->connection, hdr->value, MAX_HEADER_VALUE_LENGTH);
            }
            else if (strcmp(hdr->key, "Proxy-Connection:")==0)
            {
                strncpy(header->proxy_connection,
                        hdr->value, MAX_HEADER_VALUE_LENGTH);
            }
            else
            {
                add_new_header_item(header, hdr);
                other_headers_scanned = true;
                continue;
            }
            /* If the header is not added to the list, free it.
             * Some of the headers are not added to the list but instead
             * directly stored in the structure such as user agent, connection
             * host, proxy connection.*/
            Free(hdr);
            other_headers_scanned = true;
        }
        else
        {
            /* Parse not success, free the header */
            Free(hdr);
            if (strcmp(temp_buffer, "\r\n") == 0)
            {
                other_headers_scanned = true;
                break;
            }
            else
            {
                printf("ERROR: Invalid header key values:%s:\n", temp_buffer);
                return HTTP_ERR_HEADER_KEY_VALUE_INVALID;
            }
        }
    }
    if (!(request_format_scanned && other_headers_scanned))
    {
        return HTTP_INVALID_PROTOCOL;
    }
    return SUCCESS;
}
