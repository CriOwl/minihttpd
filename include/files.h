#ifndef FILE_H
#define FILE_H
#include <stddef.h>

struct file_metadata{
    int fd_file;
    size_t len_file;
    long int offset;
    int is_file_not_send;

};
int init_files(void);
int send_body_http(int fd_socket, struct file_metadata *fil);
int open_fd_get_len(struct file_metadata *fil,char *path, size_t idx_path);

#endif
