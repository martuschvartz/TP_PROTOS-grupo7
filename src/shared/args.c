#include <stdio.h>  /* for printf */
#include <stdlib.h> /* for exit */
#include <limits.h> /* LONG_MIN et al */
#include <string.h> /* memset */
#include <errno.h>
#include <getopt.h>
#include <args.h>
#include "logger.h"

static unsigned short
port(const char *s)
{
    char *end = 0;
    const long sl = strtol(s, &end, 10);

    if (end == s || '\0' != *end || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno) || sl < 0 || sl > USHRT_MAX)
    {
        StringBuilder *sb = sb_create();
        sb_append(sb, "Port should be in the range of 1-65536: ");
        sb_append(sb, s);
        our_log(ERROR, sb_get_string(sb));
        sb_free(sb);
        exit(1);
        return 1;
    }
    return (unsigned short)sl;
}

static void
user(char *s, struct users *user)
{
    char *p = strchr(s, ':');
    if (p == NULL)
    {
        StringBuilder *sb = sb_create();
        sb_append(sb, "Password not found");
        sb_append(sb, s);
        our_log(WARNING, sb_get_string(sb));
        sb_free(sb);
        exit(1);
    }
    else
    {
        *p = 0;
        p++;
        user->name = s;
        user->pass = p;
    }
}

static void
version(void)
{
    fprintf(stderr, "socks5v version 0.0\n"
                    "ITBA Protocolos de Comunicación 2025/1 -- Grupo 7\n"
                    "AQUI VA LA LICENCIA\n");
}

static void
usage(const char *progname)
{
    fprintf(stderr,
            "Usage: %s [OPTION]...\n"
            "\n"
            "   -h               Imprime la ayuda y termina.\n"
            "   -l <SOCKS addr>  Dirección donde servirá el proxy SOCKS.\n"
            "   -L <conf  addr>  Dirección donde servirá el servicio de management.\n"
            "   -p <SOCKS port>  Puerto entrante conexiones SOCKS.\n"
            "   -P <conf port>   Puerto entrante conexiones configuracion\n"
            "   -u <name>:<pass> Usuario y contraseña de usuario que puede usar el proxy. Hasta 10.\n"
            "   -v               Imprime información sobre la versión versión y termina.\n"

            "\n",
            progname);
    exit(1);
}

void parse_args(const int argc, char **argv, struct socks5args *args)
{
    memset(args, 0, sizeof(*args)); // sobre todo para setear en null los punteros de users

    args->socks_addr = "0.0.0.0";
    args->socks_port = 1080;

    args->mng_addr = "127.0.0.1";
    args->mng_port = 1081;

    args->disectors_enabled = true;

    int c;
    int nusers = 0;

    while (true)
    {
        int option_index = 0;
        static struct option long_options[] = {
            {0, 0, 0, 0}};

        c = getopt_long(argc, argv, "hl:L:Np:P:u:v", long_options, &option_index);
        if (c == -1)
            break;

        switch (c)
        {
        case 'h':
            usage(argv[0]);
            break;
        case 'l':
            args->socks_addr = optarg;
            break;
        case 'L':
            args->mng_addr = optarg;
            break;
        case 'N':
            args->disectors_enabled = false;
            break;
        case 'p':
            args->socks_port = port(optarg);
            break;
        case 'P':
            args->mng_port = port(optarg);
            break;
        case 'u':
            if (nusers >= MAX_USERS)
            {
                StringBuilder *sb = sb_create();
                sb_append(sb, "Maximun number of command line users reached: ");
                char * str = int_to_string(MAX_USERS);
                sb_append(sb, str);
                free(str);
                our_log(WARNING, sb_get_string(sb));
                sb_free(sb);
                exit(1);
            }
            else
            {
                user(optarg, args->users + args->cant);
                args->cant++;
            }
            break;
        case 'v':
            version();
            exit(0);
        default:
            {
            StringBuilder *sb2 = sb_create();
            sb_append(sb2, "Unknown argument: ");
            char * str = int_to_string(c);
            sb_append(sb2, str);
            free(str);
            our_log(ERROR, sb_get_string(sb2));
            sb_free(sb2);
            exit(1);
            }
        }
    }
    if (optind < argc)
    {
        StringBuilder *sb = sb_create();
        sb_append(sb, "Argument not accepted:");
        while (optind < argc)
        {
            sb_append(sb, argv[optind++]);
        }
        our_log(WARNING, sb_get_string(sb));
        sb_free(sb);
                exit(1);
    }
}
