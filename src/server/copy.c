#include <copy.h>
#include <errno.h>
#include <selector.h>
#include <socks5.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>


static unsigned handle_copy_read(selector_key *key, int source_fd, int dest_fd, buffer * target_buffer) {
    size_t bytes_available;
    uint8_t *ptr = buffer_write_ptr(target_buffer, &bytes_available);

    if (!buffer_can_write(target_buffer)) {
        // avoid polling ?
        selector_set_interest(key->s, source_fd, OP_NOOP);
        return COPY_READ;
    }

    ssize_t bytes_read = recv(key->fd, ptr, bytes_available, 0);

    if (bytes_read == 0) {
        fprintf(stdout, "Side %d closed the connection\n", source_fd);
        return SOCKS_DONE;
    }
    if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        return SOCKS_ERROR;
    }
    buffer_write_adv(target_buffer, bytes_read);
    if (buffer_can_read(target_buffer) && selector_set_interest(key->s, dest_fd, OP_WRITE) != SELECTOR_SUCCESS) {
        return SOCKS_ERROR;
    }

    return COPY_READ;
}

static unsigned handle_copy_write(selector_key * key, int source_fd, int dest_fd, buffer * source_buffer) {
    size_t bytes_available;
    uint8_t *ptr = buffer_read_ptr(source_buffer, &bytes_available);

    if (!buffer_can_read(source_buffer)) {
        // avoid polling ?
        selector_set_interest(key->s, source_fd, OP_NOOP);
        return COPY_READ;
    }

    ssize_t bytes_sent = recv(key->fd, ptr, bytes_available, 0);

    if (bytes_read == 0) {
        fprintf(stdout, "Side %d closed the connection\n", source_fd);
        return SOCKS_DONE;
    }
    if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        return SOCKS_ERROR;
    }
    buffer_write_adv(info_from_client, bytes_read);
    if (buffer_can_read(info_from_client) && selector_set_interest(key->s, dest_fd, OP_WRITE) != SELECTOR_SUCCESS) {
        return SOCKS_ERROR;
    }

    return COPY_READ;
}


unsigned int copy_read(selector_key *key)
{
    puts("im readng");
    client_data *client_data = ATTACHMENT(key);
    size_t available;
    uint8_t *ptr = buffer_write_ptr(&client_data->client_to_sv, &available);
    if (!buffer_can_write(&client_data->client_to_sv)) {
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
    buffer_write_adv(&client_data->client_to_sv, n);
    if(n<BUFFER_SIZE){
        selector_set_interest(key->s, key->fd, OP_WRITE);
    }
    return COPY_WRITE;
}

// non blocking send
unsigned int copy_write(selector_key *key)
{
    puts("im writing");
    client_data *client_data = ATTACHMENT(key);
    size_t available;
    uint8_t *ptr = buffer_read_ptr(&client_data->client_to_sv, &available);

    if (!buffer_can_read(&client_data->client_to_sv)) {
        // Nothing to write
        selector_set_interest(key->s, key->fd, OP_READ);
        return SOCKS_ERROR;
    }

    ssize_t n = send(key->fd, ptr, available, 0);
    if (n <= 0) {
        return SOCKS_DONE;
    }

    buffer_read_adv(&client_data->client_to_sv, n);
    selector_set_interest(key->s, key->fd, OP_READ);
    return COPY_READ;
}