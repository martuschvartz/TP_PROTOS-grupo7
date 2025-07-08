#ifndef NEGOTIATION_H
#define NEGOTIATION_H
#include <selector.h>
#include <socks5.h>
#include <negotiation_parser.h>
#include <sys/socket.h>
#include <stdio.h>

void negotiation_read_init(const unsigned state, selector_key * key);

unsigned int negotiation_write(selector_key * key);

unsigned int negotiation_read(selector_key * key);

#endif
