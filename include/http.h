#ifndef HTTP_H
#define HTTP_H
#include <stddef.h>
#include "files.h"

#define BUFFER_READ         40960
#define MAX_HEADER             30
#define MAX_BUFFER_HEADER   4096
#define MAX_BUFFER_VALUE    4096
#define MAX_METHOD            15
#define MAX_PATH             120


struct http_header_ {
    char header[MAX_BUFFER_HEADER];
    size_t idx_header;
    char value[MAX_BUFFER_VALUE];
    size_t idx_value;
};

struct first_line {
    char method[MAX_METHOD];
    size_t idx_method;
    char path[MAX_PATH];
    size_t idx_path;
};

struct http_msg_ {
    char buffer_read[BUFFER_READ];
    size_t idx_begin;
    int idx_end;
    size_t len_read;
    struct first_line f_line;
    struct http_header_ headers_t[MAX_HEADER];
    size_t idx_header;
    int close_after_response;
    struct file_metadata send_file;
};

struct client{
    int fd_clt;
    struct http_msg_ http_msg;
};

void http_init(struct http_msg_ *http_msg);
int http_write(struct http_msg_ *http_msg, char *data, size_t len_data);
int response(struct client *clt, int fd, int code_http);
#endif
