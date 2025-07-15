#include <copy.h>
#include <errno.h>
#include <selector.h>
#include <socks5.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>


static unsigned int handle_read_data(selector_key *key, ssize_t n, buffer *buffer_to_fill, int target_fd) {
    client_data *data = ATTACHMENT(key);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return COPY;
        }
        return SOCKS_ERROR;
    }

    if (n == 0) {
        fprintf(stdout, "read 0 bytes from source %d", target_fd);
        shutdown(key->fd, SHUT_RD);

        if (key->fd == data->client_fd) {
            data->client_fd = -1;
            if (!buffer_can_read(&data->client_to_sv) && data->origin_fd != -1) {
                shutdown(data->origin_fd, SHUT_WR);
                data->origin_fd = -1;
            }
        } else {
            data->origin_fd = -1;
            if (!buffer_can_read(&data->sv_to_client) && data->client_fd != -1) {
                shutdown(data->client_fd, SHUT_WR);
            }
        }

        if ((data->client_fd == -1 && !buffer_can_read(&data->client_to_sv)) &&
            (data->origin_fd == -1 && !buffer_can_read(&data->sv_to_client))) {
            return SOCKS_DONE;
        }
        return COPY;
    }

    buffer_write_adv(buffer_to_fill, n);

    if (target_fd != -1) {
        selector_set_interest(key->s, target_fd, OP_WRITE);
    }

    return COPY;
}

unsigned int copy_read(selector_key *key) {
    fprintf(stdout, "[COPY_READ] key->fd: %d\n", key->fd);
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
        return COPY;
    }

    ssize_t n = recv(key->fd, ptr, available_write_space, 0);

    fprintf(stdout, "[COPY_READ] AFTER RECV key->fd: %d\n", key->fd);

    return handle_read_data(key, n, buffer_to_fill, target_fd);
}


static unsigned int handle_write_data(selector_key *key, ssize_t n, buffer *buffer_to_drain, int source_fd) {
    client_data *data = ATTACHMENT(key);

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
        }
    }

    if (!buffer_can_read(buffer_to_drain)) {
        if (key->fd == data->origin_fd) {
            selector_set_interest(key->s, key->fd, OP_READ);
        }

        if (source_fd == -1) {
            shutdown(key->fd, SHUT_WR);
        }

        if (key->fd == data->origin_fd && !buffer_can_read(&data->client_to_sv) && data->client_fd == -1) {
            fprintf(stdout, "[COPY_WRITE] Origin finished writing, client is gone. Closing origin.\n");
            shutdown(data->origin_fd, SHUT_WR);
            return SOCKS_DONE;
    }

        if (key->fd == data->client_fd && !buffer_can_read(&data->sv_to_client) && data->client_fd == -1) {
            fprintf(stdout, "[COPY_WRITE] Client transaction complete (client_fd closed and buffer empty). Returning SOCKS_DONE. inside handle write\n");
            if (data->origin_fd != -1) {
                shutdown(data->origin_fd, SHUT_WR);
            }
            return SOCKS_DONE;
        }

        if ((data->client_fd == -1 && !buffer_can_read(&data->client_to_sv)) &&
            (data->origin_fd == -1 && !buffer_can_read(&data->sv_to_client))) {
            return SOCKS_DONE;
        }
    } else {
        selector_set_interest(key->s, key->fd, OP_WRITE);
    }

    return COPY;
}

unsigned int copy_write(selector_key *key) {
    fprintf(stdout, "[COPY_WRITE] key->fd: %d\n", key->fd);
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
        selector_set_interest(key->s, key->fd, OP_READ);

        if (source_fd == -1) {
            shutdown(key->fd, SHUT_WR);
        }

        if (key->fd == data->client_fd && !buffer_can_read(&data->sv_to_client) && data->client_fd == -1) {
            fprintf(stdout, "[COPY_WRITE] Client transaction complete (client_fd closed and buffer empty). Returning SOCKS_DONE.\n");
            if (data->origin_fd != -1) {
                shutdown(data->origin_fd, SHUT_WR);
            }
            return SOCKS_DONE;
        }

        if ((data->client_fd == -1 && !buffer_can_read(&data->client_to_sv)) &&
            (data->origin_fd == -1 && !buffer_can_read(&data->sv_to_client))) {
            return SOCKS_DONE;
        }
        return COPY;
    }

    ssize_t n = send(key->fd, ptr, available_read_data, MSG_NOSIGNAL);

    return handle_write_data(key, n, buffer_to_drain, source_fd);
}

