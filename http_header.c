/* Functions to process HTTP headers.
 * Author: Vamshi Reddy Konagar (vkonagar)
 * Date: 12/5/2016
 * Email: vkonagar@andrew.cmu.edu
 */
#include "http_header.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "csapp.h"

/* add_new_header_item
 * Inserts the header pair (key, value) item into the http header
 * structure.
 * @param http_header header's address.
 * @param hdr key value pair.
 */
void add_new_header_item(http_header_t* http_header, header_kv_pair_t* hdr)
{
    /* This key value pair, hdr, should go into the other headers in
     * the HTTP structure */
    if (http_header->other_headers == NULL)
    {
        /* First insertion */
        http_header->other_headers = hdr;
        hdr->next = NULL;
        return;
    }

    /* Insert at the end */
    header_kv_pair_t* temp = http_header->other_headers;
    while (temp->next)
    {
        temp = temp->next;
    }
    temp->next = hdr;
    hdr->next = NULL;
    return;
}

/* free_kvpairs_in_header
 * frees the header's key value pairs */
void free_kvpairs_in_header(http_header_t* header)
{
    header_kv_pair_t* cur = header->other_headers;
    while(cur)
    {
        header_kv_pair_t* old = cur;
        cur = cur->next;
        Free(old);
    }
    return;
}

/* init_header
 * initialize the header */
void init_header(http_header_t* header)
{
    /* Set all the values to 0 */
    memset(header->request_type, 0, MAX_REQUEST_TYPE_LENGTH);
    memset(header->request_url, 0, MAX_URL_LENGTH);
    memset(header->request_http_version, 0, MAX_HTTP_VERSION_LENGTH);
    memset(header->host, 0, MAX_HEADER_VALUE_LENGTH);
    memset(header->user_agent, 0, MAX_HEADER_VALUE_LENGTH);
    memset(header->connection, 0, MAX_HEADER_VALUE_LENGTH);
    memset(header->proxy_connection, 0, MAX_HEADER_VALUE_LENGTH);
    header->other_headers = NULL;
}

