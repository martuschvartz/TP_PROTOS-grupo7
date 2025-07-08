#include <echo.h>
#include <selector.h>
#include <socks5.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

unsigned int echo_read(selector_key *key)
{
    puts("im readng");
    client_data *client_data = ATTACHMENT(key);
    size_t available;
    uint8_t *ptr = buffer_write_ptr(&client_data->client.echo.bf, &available);
    if (!buffer_can_write(&client_data->client.echo.bf)) {
        // buffer is full, drop or wait
        return SOCKS_ERROR;
    }

    const ssize_t n = recv(key->fd, ptr, available, 0);
    printf("recv returned %zd\n", n);
    if (n <= 0) {
        printf("Client disconnected (fd=%d)\n", key->fd);
        return SOCKS_DONE;
    }

    printf("didn't return\n");
    buffer_write_adv(&client_data->client.echo.bf, n);
    if(n<BUFFER_SIZE){
        selector_set_interest(key->s, key->fd, OP_WRITE);
    }
    return ECHO_WRITE;
}

// non blocking send
unsigned int echo_write(selector_key *key)
{
    puts("im writing");
    client_data *client_data = ATTACHMENT(key);
    size_t available;
    uint8_t *ptr = buffer_read_ptr(&client_data->client.echo.bf, &available);

    if (!buffer_can_read(&client_data->client.echo.bf)) {
        // Nothing to write
        selector_set_interest(key->s, key->fd, OP_READ);
        return ERROR;
    }

    ssize_t n = send(key->fd, ptr, available, 0);
    if (n <= 0) {
        return DONE;
    }

    buffer_read_adv(&client_data->client.echo.bf, n);
    selector_set_interest(key->s, key->fd, OP_READ);
    return ECHO_READ;
}

unsigned int echo_close(selector_key *key)
{
    client_data *client_data = ATTACHMENT(key);
    free(client_data);
    close(key->fd);
    return DONE;
}