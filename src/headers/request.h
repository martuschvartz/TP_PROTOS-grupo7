#ifndef REQUEST_H
#define REQUEST_H
#include <selector.h>

#include "socks5.h"

typedef struct resolve_job {
    fd_selector selector;
    int client_fd;
    client_data * client_data;
}resolve_job;

void request_read_init(const unsigned state, selector_key * key);

unsigned request_read(selector_key * key);

unsigned request_write(selector_key * key);

unsigned request_resolve(selector_key * key);

void request_connect_init(const unsigned state, selector_key * key);

unsigned request_connect(selector_key * key);

#endif //REQUEST_H
