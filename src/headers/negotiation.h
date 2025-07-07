#ifndef NEGOTIATION_H
#define NEGOTIATION_H
#include <selector.h>
#include <socks5.h>
#include <negotiation_parser.h>

void negotiation_read_init(const unsigned state, selector_key * key);

void negotiation_write(selector_key * key);

void negotiation_read(selector_key * key);

#endif
