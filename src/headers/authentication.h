#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#include <authentication_parser.h>
#include <selector.h>
#include <socks5.h>
#include <stdio.h>
#include <sys/socket.h>

void authentication_read_init(const unsigned state, selector_key * key);

unsigned authentication_read(selector_key * key);

unsigned authentication_write(selector_key * key);

#endif //AUTHENTICATION_H
