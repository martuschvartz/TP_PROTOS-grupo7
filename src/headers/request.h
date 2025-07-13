#ifndef REQUEST_H
#define REQUEST_H
#include <selector.h>

void request_read_init(const unsigned state, selector_key * key);

unsigned request_read(selector_key * key);

unsigned request_write(selector_key * key);

//for connection?

#endif //REQUEST_H
