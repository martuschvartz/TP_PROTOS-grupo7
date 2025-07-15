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


static bool done = false;

static void sigterm_handler(const int signal){
    StringBuilder *sb = sb_create();
    sb_append(sb, "Signal ");
    sb_append(sb, int_to_string(signal));
    sb_append(sb, ", cleaning up and exiting");
    our_log(INFO, sb_get_string(sb));
    sb_free(sb);
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
int set_server_sock_address(int port, void *res_address, int *res_address_length, char * sock_addrs){
    //si es ipv6 tiene ':'
    int ipv6 = strchr(sock_addrs, ':') != NULL;
    if(ipv6){
        struct sockaddr_in6 sock_ipv6;
        memset(&sock_ipv6, 0, sizeof(sock_ipv6));
        sock_ipv6.sin6_family= AF_INET6;
        inet_pton(AF_INET6, sock_addrs, &sock_ipv6.sin6_addr);
        sock_ipv6.sin6_port= htons(port);

        *((struct sockaddr_in6 *)res_address) = sock_ipv6;
        *res_address_length = sizeof(sock_ipv6);
        return 0;
    }

    struct sockaddr_in sock_ipv4;
    memset(&sock_ipv4, 0, sizeof(sock_ipv4));
    sock_ipv4.sin_family = AF_INET;
    inet_pton(AF_INET, sock_addrs, &sock_ipv4.sin_addr.s_addr);
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

    create_logs_sb();
    struct socks5args socksArgs;
    init_users();
    socksArgs.cant++;
    parse_args(argc, argv, &socksArgs);
    for (int i = 0; i < socksArgs.cant; i++){
        new_user(socksArgs.users[i].name, socksArgs.users[i].pass);
    }

    if (set_server_sock_address(socksArgs.socks_port, &server_addr, &server_addr_len, socksArgs.socks_addr)){
        err_msg = "Invalid server socket address";
        goto finally;
    }

    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server < 0){
        err_msg = "Unable to create server socket";
        goto finally;
    }

    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    if (bind(server, (struct sockaddr *)&server_addr, server_addr_len) < 0){
        err_msg = "Unable to bind socket";
        goto finally;
    }

    if (listen(server, MAX_CLIENTS) < 0){
        err_msg = "Unable to listen";
        goto finally;
    }

    /* --------------------------------------------------------------------------------*/

    StringBuilder *sb = sb_create();
    sb_append(sb, "Listening on TCP port ");
    sb_append(sb, int_to_string(socksArgs.socks_port));
    our_log(INFO, sb_get_string(sb));
    sb_free(sb);

    struct sockaddr_storage manager_addr;
    int manager_addr_len;
    memset(&manager_addr, 0, sizeof(manager_addr));
    manager_addr_len = sizeof(manager_addr);

    if (set_server_sock_address(socksArgs.mng_port, &manager_addr, &manager_addr_len, socksArgs.socks_addr)){
        err_msg = "Invalid manager socket address";
        goto finally;
    }

    const int manager = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (manager < 0){
        err_msg = "Unable to create manager socket";
        goto finally;
    }

    setsockopt(manager, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    if (bind(manager, (struct sockaddr *)&manager_addr, manager_addr_len) < 0){
        err_msg = "Unable to bind socket";
        goto finally;
    }

    if (listen(manager, MAX_CLIENTS) < 0){
        err_msg = "Unable to listen";
        goto finally;
    }

    StringBuilder *sb2 = sb_create();
    sb_append(sb2, "Listening on TCP port ");
    sb_append(sb2, int_to_string(socksArgs.mng_port));
    our_log(INFO, sb_get_string(sb2));
    sb_free(sb2);

    /* ------------------------------------------------------------------------------*/

    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    if (selector_fd_set_nio(server) == -1){
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
    if (0 != selector_init(&conf)){
        err_msg = "Unable to initialize selector";
        goto finally;
    }

    selector = selector_new(1024);
    if (selector == NULL){
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

    if (selector_status != SELECTOR_SUCCESS){
        err_msg = "Unable to register server socket";
        goto finally;
    }
    while (!done){
        err_msg = NULL;
        selector_status = selector_select(selector);
        if (selector_status != SELECTOR_SUCCESS){
            err_msg = "Serving";
            goto finally;
        }
    }

    if (err_msg == NULL){
        err_msg = "Closing server";
    }

    int ret = 0;

finally:
    close_users();
    log_free();

    if (selector_status != SELECTOR_SUCCESS){
        StringBuilder *sb = sb_create();
        sb_append(sb, (err_msg == NULL) ? "" : err_msg);
        sb_append(sb, ": ");
        sb_append(sb, selector_status == SELECTOR_IO ? strerror(errno) : selector_error(selector_status));
        our_log(ERROR, sb_get_string(sb));
        sb_free(sb);
        ret = 2;
    }
    else if (err_msg){
        perror(err_msg);
        ret = 1;
    }
    if (selector != NULL){
        selector_destroy(selector);
    }
    selector_close();

    if (server >= 0){
        close(server);
    }
    if (manager >= 0){
        close(manager);
    }
    return ret;
}
