#ifndef HTTP_HEADER_PROXY_H
#define HTTP_HEADER_PROXY_H

/* These two macros should be consistent */
#define MAX_URL_LENGTH 100

/* Port number string max length */
#define PORT_NUMBER_STR_MAX_LENGTH 10

/* HTTP header max string lengths */
#define MAX_HEADER_KEY_LENGTH 500
#define MAX_HEADER_VALUE_LENGTH 500
#define MAX_REQUEST_TYPE_LENGTH 20
#define MAX_HTTP_VERSION_LENGTH 20

/* Total HTTP header string length */
#define MAX_HEADERS_TOTAL_LENGTH 10000

/* HTTP parsing error codes */
#define SUCCESS 0
#define HTTP_REQ_TYPE_NOT_SUPPORTED 1
#define HTTP_VERSION_NOT_SUPPORTED 2
#define HTTP_INVALID_REQUEST 3
#define HTTP_ERR_HEADER_KEY_VALUE_INVALID 4
#define HTTP_PARSE_ERROR 5
#define HTTP_INVALID_PROTOCOL 6

/* HTTP response codes for errors */
#define HTTP_ERR_CODE_BAD_REQUEST 400
#define HTTP_ERR_CODE_METHOD_NOT_ALLOWED 405

/* Max error message string length */
#define MAX_ERROR_MSG_LENGTH 30

/* Max line that can be read into a buffer */
#define MAX_READLINE_STR_LENGTH 5000

/* Macros to form format specifier strings */
#ifndef STRINGIFY
#define STRINGIFY2( x) #x
#define STRINGIFY(x) STRINGIFY2(x)
#endif
#define STR_FMTB(x) "%" STRINGIFY(x) "s"

/* Key values of the HTTP header. Used in a singly linked list  */
typedef struct header_key_value
{
    char key[MAX_HEADER_KEY_LENGTH];
    char value[MAX_HEADER_VALUE_LENGTH];
    struct header_key_value* next;
}header_kv_pair_t;

/* HTTP header structure with header_key_values */
typedef struct http_header
{
    /* HTTP Request line. Ex: GET / HTTP/1.1 */
    char request_type[MAX_REQUEST_TYPE_LENGTH]; /* GET */
    char request_url[MAX_URL_LENGTH]; /* /, /index.html, etc */
    char request_http_version[MAX_HTTP_VERSION_LENGTH]; /* HTTP/1.1, HTTP/1.0 */
    /* Most useful HTTP headers for proxy */
    char host[MAX_HEADER_VALUE_LENGTH];
    char user_agent[MAX_HEADER_VALUE_LENGTH];
    char connection[MAX_HEADER_VALUE_LENGTH];
    char proxy_connection[MAX_HEADER_VALUE_LENGTH];
    /* Other headers present in the request/response */
    header_kv_pair_t *other_headers;
}http_header_t;

/* Adds a new header item (key-value pair) to the http header structure */
void add_new_header_item(http_header_t* http_header, header_kv_pair_t* hdr);
/* Frees a http header's key value pairs */
void free_kvpairs_in_header(http_header_t* header);
/* Initializes the HTTP header */
void init_header(http_header_t* header);

#endif /* HTTP_HEADER_PROXY_H */
