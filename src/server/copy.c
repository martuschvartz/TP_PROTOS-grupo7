#include <copy.h>
#include <errno.h>
#include <selector.h>
#include <socks5.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

unsigned int copy_read(selector_key *key) {
    printf("in copy read\n");
    client_data *data = ATTACHMENT(key);
    buffer *buffer_to_fill;
    int target_fd;

    if (key->fd == data->client_fd) {
        buffer_to_fill = &data->client_to_sv;
        target_fd = data->origin_fd;
    } else if (key->fd == data->origin_fd) {
        buffer_to_fill = &data->sv_to_client;
        target_fd = data->client_fd;
    } else {
        return SOCKS_ERROR;
    }

    size_t available_write_space;
    uint8_t *ptr = buffer_write_ptr(buffer_to_fill, &available_write_space);


    if (available_write_space == 0) {
        selector_set_interest(key->s, key->fd, OP_NOOP);
        if (target_fd != -1 && buffer_can_read(buffer_to_fill)) {
             selector_set_interest(key->s, target_fd, OP_WRITE);
        }
        return COPY;
    }

    ssize_t n = recv(key->fd, ptr, available_write_space, 0);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return COPY;
        }
        return SOCKS_ERROR;
    }
    if (n == 0) {
        if (key->fd == data->client_fd) {
            shutdown(data->client_fd, SHUT_RD);
            data->client_fd = -1;
        } else {
            shutdown(data->origin_fd, SHUT_RD);
            data->origin_fd = -1;
        }
        selector_set_interest(key->s, key->fd, OP_NOOP);

        if ( (data->client_fd == -1 && !buffer_can_read(&data->client_to_sv)) &&
             (data->origin_fd == -1 && !buffer_can_read(&data->sv_to_client)) ) {
            return SOCKS_DONE;
        }
        return COPY;
    }

    buffer_write_adv(buffer_to_fill, n);

    if (target_fd != -1) {
        selector_set_interest(key->s, target_fd, OP_WRITE);
    }

    if (!buffer_can_write(buffer_to_fill)) {
        selector_set_interest(key->s, key->fd, OP_NOOP);
    } else {
        selector_set_interest(key->s, key->fd, OP_READ);
    }

    return COPY;
}

unsigned int copy_write(selector_key *key) {
    client_data *data = ATTACHMENT(key);
    buffer *buffer_to_drain;
    int source_fd;

    if (key->fd == data->client_fd) {
        buffer_to_drain = &data->sv_to_client;
        source_fd = data->origin_fd;
    } else if (key->fd == data->origin_fd) {
        buffer_to_drain = &data->client_to_sv;
        source_fd = data->client_fd;
    } else {
        return SOCKS_ERROR;
    }

    size_t available_read_data;
    uint8_t *ptr = buffer_read_ptr(buffer_to_drain, &available_read_data);

    if (available_read_data == 0) {
        selector_set_interest(key->s, key->fd, OP_NOOP);

        if ( (key->fd == data->client_fd && data->origin_fd == -1) ||
             (key->fd == data->origin_fd && data->client_fd == -1) ) {
            shutdown(key->fd, SHUT_WR);
        }

        if ( (data->client_fd == -1 && !buffer_can_read(&data->client_to_sv)) &&
             (data->origin_fd == -1 && !buffer_can_read(&data->sv_to_client)) ) {
            return SOCKS_DONE;
        }
        return COPY;
    }

    ssize_t n = send(key->fd, ptr, available_read_data, MSG_NOSIGNAL);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return COPY;
        }
        return SOCKS_ERROR;
    }
    if (n == 0) {

        return SOCKS_ERROR;
    }

    buffer_read_adv(buffer_to_drain, n);

    if (source_fd != -1) {
        if (buffer_can_write(buffer_to_drain)) {
            selector_set_interest(key->s, source_fd, OP_READ);
        } else {
            selector_set_interest(key->s, source_fd, OP_NOOP);
        }
    }


    if (!buffer_can_read(buffer_to_drain)) {
        selector_set_interest(key->s, key->fd, OP_NOOP);

        if ( (key->fd == data->client_fd && data->origin_fd == -1) ||
             (key->fd == data->origin_fd && data->client_fd == -1) ) {
            shutdown(key->fd, SHUT_WR);
        }

        if ( (data->client_fd == -1 && !buffer_can_read(&data->client_to_sv)) &&
             (data->origin_fd == -1 && !buffer_can_read(&data->sv_to_client)) ) {
            return SOCKS_DONE;
        }
    } else {
        selector_set_interest(key->s, key->fd, OP_WRITE);
    }

    return COPY;
}