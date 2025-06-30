#ifndef ARGS_H
#define ARGS_H

#include <stdbool.h>

#define MAX_USERS 10

struct users
{
    char* name;
    char* pass;
};

struct socks5args
{
    char* socks_addr;
    unsigned short socks_port;

    char* mng_addr;
    unsigned short mng_port;

    bool disectors_enabled;

    struct users users[MAX_USERS];
};

/**
 * Interpreta la linea de comandos (argc, argv) llenando
 * args con defaults o la seleccion humana. Puede cortar
 * la ejecución.
 */
void
parse_args(const int argc, char** argv, struct socks5args* args);

#endif
