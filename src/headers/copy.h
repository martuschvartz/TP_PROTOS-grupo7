#ifndef ECHO_H
#define ECHO_H

#include <selector.h>

unsigned int copy_read(selector_key *key);

unsigned int copy_write(selector_key *key);

void copy_init(const unsigned state, selector_key *key);

#endif