#ifndef ECHO_H
#define ECHO_H

#include <selector.h>

unsigned int echo_read(selector_key *key);

// non blocking send
unsigned int echo_write(selector_key *key);

unsigned int echo_close(selector_key *key);

#endif