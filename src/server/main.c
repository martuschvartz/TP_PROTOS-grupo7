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

#define MAX_CLIENTS 3
#define BUFFER_SIZE 1024

static bool done = false;

static void
sigterm_handler(const int signal)
{
    printf("signal %d, cleaning up and exiting\n", signal);
    done = true;
}

/*----------------- ECHO SERVER ------------------*/

struct client_data
{
    buffer buffer;
    uint8_t raw_buffer[BUFFER_SIZE];
};

void echo_passive_read(struct selector_key *key)
{
    puts("im readng");
    struct client_data *client_data = (struct client_data *)(key)->data;
    size_t available;
    uint8_t *ptr = buffer_write_ptr(&client_data->buffer, &available);
    if (!buffer_can_write(&client_data->buffer)) {
        // buffer is full, drop or wait
        return;
    }

    ssize_t n = recv(key->fd, ptr, available, 0);
    printf("recv returned %zd\n", n);
    if (n <= 0) {
        printf("Client disconnected (fd=%d)\n", key->fd);
        selector_unregister_fd(key->s, key->fd);
        return;
    }

    printf("didn´t return\n");
    buffer_write_adv(&client_data->buffer, n);
    selector_set_interest(key->s, key->fd, OP_WRITE);
}

// non blocking send
void echo_passive_write(struct selector_key *key)
{
    puts("im writing");
    struct client_data *client_data = (struct client_data *)(key)->data;
    size_t available;
    uint8_t *ptr = buffer_read_ptr(&client_data->buffer, &available);

    if (!buffer_can_read(&client_data->buffer)) {
        // Nothing to write
        selector_set_interest(key->s, key->fd, OP_READ);
        return;
    }

    ssize_t n = send(key->fd, ptr, available, 0);
    if (n <= 0) {
        selector_unregister_fd(key->s, key->fd);
        return;
    }

    buffer_read_adv(&client_data->buffer, n);
    selector_set_interest(key->s, key->fd, OP_READ);
}

void echo_passive_close(struct selector_key *key)
{
    struct client_data *client_data = (struct client_data *)(key)->data;
    free(client_data);
    close(key->fd);
}


const struct fd_handler socksv5 = {
        .handle_read = echo_passive_read,
        .handle_write = echo_passive_write,
        .handle_close = echo_passive_close,
    };

// note, do free of client data
void echo_passive_accept(struct selector_key *key)
{
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    const int client = accept(key->fd, (struct sockaddr *)&client_addr,
                              &client_addr_len);
    if (client == -1)
    {
        return;
    }
    if (selector_fd_set_nio(client) == -1)
    {
        return;
    }

    struct client_data *client_data = calloc(1, sizeof(struct client_data));
    if (client_data == NULL)
    {
        close(client);
        return;
    }
    buffer_init(&client_data->buffer, BUFFER_SIZE, client_data->raw_buffer);

    if (SELECTOR_SUCCESS != selector_register(key->s, client, &socksv5,
                                              OP_READ, client_data))
    {
        close(client);
        return;
    }
    return;
}

/*-----------------------------------------------------------------*/

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
    struct sockaddr_in sock_ipv4;
    memset(&sock_ipv4, 0, sizeof(sock_ipv4));

    sock_ipv4.sin_family = AF_INET;
    sock_ipv4.sin_addr.s_addr = htonl(INADDR_ANY);
    sock_ipv4.sin_port = htons(port);

    *((struct sockaddr_in *)res_address) = sock_ipv4;
    *res_address_length = sizeof(sock_ipv4);

    return 0;
}

int main(void)
{
    int port = 1080;
    selector_status selector_status = SELECTOR_SUCCESS;
    const char *err_msg = NULL;
    fd_selector selector = NULL;

    struct sockaddr_storage server_addr;
    int server_addr_len;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr_len = sizeof(server_addr);

    if (set_server_sock_address(port, &server_addr, &server_addr_len))
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

    fprintf(stdout, "Listenting on TCP port %d\n", port);

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
        .handle_read = echo_passive_accept,
        .handle_write = NULL,
        .handle_close = NULL,
    };

    selector_status = selector_register(selector, server, &socksv5,
                                        OP_READ, NULL);
    if (selector_status != SELECTOR_SUCCESS)
    {
        err_msg = "Unable to register server socket";
        goto finally;
    }
    for (; !done;)
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
        // selector_destroy(selector);
    }
    selector_close();

    // socksv5_pool_destroy();

    if (server >= 0)
    {
        close(server);
    }
    return ret;
}
