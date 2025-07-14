#ifndef ECHO_H
#define ECHO_H

#include <selector.h>

unsigned int copy_read(selector_key *key);

// non blocking send
unsigned int copy_write(selector_key *key);


#endif