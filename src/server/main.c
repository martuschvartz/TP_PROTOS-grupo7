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
#include <socks5.h>
#include "args.h"
#include "users.h"
#include <manager.h>

#define MAX_CLIENTS 3 // 500??
#define BUFFER_SIZE 1024

static bool done = false;

static void sigterm_handler(const int signal)
{
    printf("signal %d, cleaning up and exiting\n", signal);
    done = true;
}

// note, will throw error when passing addres -> inet_ptons

/*
 *  Sets server passive socket address, ipv4 format only
 *  @param port, in which port it will listen
 *  @param res_address, address structure result
 *  @param res_address_length, address length result
 *  returns 1 error, 0 on success
 */
int set_server_sock_address(int port, void *res_address, int *res_address_length)
{
    // acá tmb ipv6 TODO
    struct sockaddr_in sock_ipv4;
    memset(&sock_ipv4, 0, sizeof(sock_ipv4));
    sock_ipv4.sin_family = AF_INET;
    sock_ipv4.sin_addr.s_addr = htonl(INADDR_ANY);
    sock_ipv4.sin_port = htons(port);

    *((struct sockaddr_in *)res_address) = sock_ipv4;
    *res_address_length = sizeof(sock_ipv4);

    return 0;
}

int main(int argc, char **argv)
{
    selector_status selector_status = SELECTOR_SUCCESS;
    const char *err_msg = NULL;
    fd_selector selector = NULL;

    struct sockaddr_storage server_addr;
    int server_addr_len;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr_len = sizeof(server_addr);

    // init logger TODO, va si o si dsp de tener un selector
    // sirve tener el logger antes del parse para ya poder ir juntando datos
    struct socks5args socksArgs;
    init_users();
    socksArgs.cant++; // ya que cree el admin con la funcion initUsers
    parse_args(argc, argv, &socksArgs);
    for (int i = 0; i < socksArgs.cant; i++)
    {
        new_user(socksArgs.users[i].name, socksArgs.users[i].pass);
    }

    if (set_server_sock_address(socksArgs.socks_port, &server_addr, &server_addr_len))
    {
        err_msg = "Invalid server socket address";
        goto finally;
    }

    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server < 0)
    {
        err_msg = "Unable to create server socket";
        goto finally;
    }

    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    if (bind(server, (struct sockaddr *)&server_addr, server_addr_len) < 0)
    {
        err_msg = "Unable to bind socket";
        goto finally;
    }

    if (listen(server, MAX_CLIENTS) < 0)
    {
        err_msg = "Unable to listen";
        goto finally;
    }

    /* --------------------------------------------------------------------------------*/

    fprintf(stdout, "Listening on TCP port %d\n", socksArgs.socks_port);

    struct sockaddr_storage manager_addr;
    int manager_addr_len;
    memset(&manager_addr, 0, sizeof(manager_addr));
    manager_addr_len = sizeof(manager_addr);

    if (set_server_sock_address(socksArgs.mng_port, &manager_addr, &manager_addr_len))
    {
        err_msg = "Invalid manager socket address";
        goto finally;
    }

    const int manager = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (manager < 0)
    {
        err_msg = "Unable to create manager socket";
        goto finally;
    }

    setsockopt(manager, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    if (bind(manager, (struct sockaddr *)&manager_addr, manager_addr_len) < 0)
    {
        err_msg = "Unable to bind socket";
        goto finally;
    }

    if (listen(manager, MAX_CLIENTS) < 0)
    {
        err_msg = "Unable to listen";
        goto finally;
    }

    fprintf(stdout, "Listening on TCP port %d\n", socksArgs.mng_port);

    /* ------------------------------------------------------------------------------*/

    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    if (selector_fd_set_nio(server) == -1)
    {
        err_msg = "Unable to set server socket to nonblock";
        goto finally;
    }
    const struct selector_init conf = {
        .signal = SIGALRM,
        .select_timeout = {
            .tv_sec = 10,
            .tv_nsec = 0,
        },
    };
    if (0 != selector_init(&conf))
    {
        err_msg = "Unable to initialize selector";
        goto finally;
    }

    selector = selector_new(1024);
    if (selector == NULL)
    {
        err_msg = "unable to create selector";
        goto finally;
    }

    const struct fd_handler socksv5 = {
        .handle_read = socks_v5_passive_accept,
        .handle_write = NULL,
        .handle_close = NULL,
    };
    selector_status = selector_register(selector, server, &socksv5, OP_READ, NULL);

    const struct fd_handler managerDory = {
        .handle_read = manager_passive_accept,
        .handle_write = NULL,
        .handle_close = NULL,
    };

    selector_status = selector_register(selector, manager, &managerDory, OP_READ, NULL);

    if (selector_status != SELECTOR_SUCCESS)
    {
        err_msg = "Unable to register server socket";
        goto finally;
    }
    while (!done)
    {
        err_msg = NULL;
        selector_status = selector_select(selector);
        if (selector_status != SELECTOR_SUCCESS)
        {
            err_msg = "Serving";
            goto finally;
        }
    }

    if (err_msg == NULL)
    {
        err_msg = "Closing server";
    }

    int ret = 0;

finally:
    close_users();
    if (selector_status != SELECTOR_SUCCESS)
    {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "" : err_msg,
                selector_status == SELECTOR_IO
                    ? strerror(errno)
                    : selector_error(selector_status));
        ret = 2;
    }
    else if (err_msg)
    {
        perror(err_msg);
        ret = 1;
    }
    if (selector != NULL)
    {
        selector_destroy(selector);
    }
    selector_close();

    if (server >= 0)
    {
        close(server);
    }
    if (manager >= 0)
    {
        close(manager);
    }
    return ret;
}
