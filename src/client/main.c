#include <stdio.h>
#include <signal.h>
#include <selector.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <buffer.h>
#include <defaults.h>
#include "args.h"
#include "users.h"
#include <manager.h>
#include <getopt.h>

#define DEFAULT_PORT 1081
#define DEFAULT_HOST "127.0.0.1"
#define BUFFER_SIZE 1024

static bool done = false;

static void stdin_read(struct selector_key *key);
static void socket_read(struct selector_key *key);
static void socket_close(struct selector_key *key);

typedef struct
{
    int sockfd;
} client_data;

static void print_help(const char *progname)
{
    printf("Uso: %s [-L host] [-P port] [-h]\n", progname);
    printf("Opciones:\n");
    printf("  -L host     Dirección IP o hostname del servidor (default: %s)\n", DEFAULT_HOST);
    printf("  -P port     Puerto del servidor (default: %d)\n", DEFAULT_PORT);
    printf("  -h          Muestra esta ayuda y termina\n");
}

static void sigterm_handler(const int signal)
{
    (void)signal;
    done = true;
}
int main(int argc, char *argv[])
{
    // const char *host = DEFAULT_HOST;
    int port = DEFAULT_PORT;

    int opt;
    while ((opt = getopt(argc, argv, "L:P:h")) != -1)
    {
        switch (opt)
        {
        case 'L':
            // host = optarg;
            break;
        case 'P':
            port = atoi(optarg);
            break;
        case 'h':
            print_help(argv[0]);
            return 0;
        default:
            print_help(argv[0]);
            return 1;
        }
    }
    /* a socket is created through call to socket() function */
    int sockfd = 0;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return 1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    /* Information like IP address of the remote host and its port is
     * bundled up in a structure and a call to function connect() is made
     * which tries to connect this socket with the socket (IP address and port)
     * of the remote host
     */
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\n Error : Connect Failed \n");
        close(sockfd);
        return 1;
    }

    /* Once the sockets are connected, the server sends the data (date+time)
     * on clients socket through clients socket descriptor and client can read it
     * through normal read call on the its socket descriptor.
     */

    // con selector ------------------------------------------------

    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    if (selector_fd_set_nio(sockfd) == -1)
    {
        printf("Unable to set server socket to nonblock");
        close(sockfd);
        return 1;
    }
    const struct selector_init conf = {
        .signal = SIGALRM,
        .select_timeout = {
            .tv_sec = 10,
            .tv_nsec = 0,
        },
    };
    if (SELECTOR_SUCCESS != selector_init(&conf))
    {
        printf("Unable to initialize selector");
        close(sockfd);
        return 1;
    }

    fd_selector selector = selector_new(BUFFER_SIZE);
    if (selector == NULL)
    {
        printf("unable to create selector");
        selector_close();
        close(sockfd);
        return 1;
    }

    client_data *data = malloc(sizeof(client_data));
    data->sockfd = sockfd;

    const struct fd_handler stdin_handler = {
        .handle_read = stdin_read,
        .handle_write = NULL,
        .handle_close = NULL,
    };

    const struct fd_handler socket_handler = {
        .handle_read = socket_read,
        .handle_write = NULL,
        .handle_close = socket_close,
    };

    selector_status st_1 = selector_register(selector, sockfd, &socket_handler, OP_READ, data);
    selector_status st_2 = selector_register(selector, STDIN_FILENO, &stdin_handler, OP_READ, data);

    if (st_1 != SELECTOR_SUCCESS || st_2 != SELECTOR_SUCCESS)
    {
        printf("Unable to register server socket");
        free(data);
        selector_destroy(selector);
        selector_close();
        close(sockfd);
        return 1;
    }

    while (!done)
    {
        if (selector_select(selector) != SELECTOR_SUCCESS)
        {
            fprintf(stderr, "Error en selector_select\n");
            break;
        }
    }

    printf("\nCerrando cliente...\n");

    selector_unregister_fd(selector, sockfd);
    selector_unregister_fd(selector, STDIN_FILENO);
    selector_destroy(selector);
    selector_close();
    close(sockfd);
    free(data);

    return 0;
}

// leer del stdin y lo mando al manager por el socket
static void stdin_read(struct selector_key *key)
{
    client_data *data = key->data;
    char buffer[BUFFER_SIZE];

    if (fgets(buffer, sizeof(buffer), stdin) == NULL)
    {
        selector_unregister_fd(key->s, STDIN_FILENO);
        return;
    }

    send(data->sockfd, buffer, strlen(buffer), 0); // mando al servidor
}

// leo lo que responde el servidor por el socket e imprimo en pantalla
static void socket_read(struct selector_key *key)
{
    client_data *data = key->data;

    char buffer[BUFFER_SIZE];
    int n = recv(data->sockfd, buffer, BUFFER_SIZE - 1, 0);
    if (n <= 0)
    {
        printf("\nServidor cerró la conexión.\n");
        selector_unregister_fd(key->s, data->sockfd);
        done = true;
        return;
    }

    buffer[n] = '\0';
    printf("%s", buffer);
    fflush(stdout);
}

static void socket_close(struct selector_key *key)
{
    client_data *data = key->data;
    if (data != NULL)
    {
        close(data->sockfd);
    }
}