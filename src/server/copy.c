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
        shutdown(key->fd, SHUT_RD);
        selector_set_interest(key->s, key->fd, OP_NOOP);

        if (key->fd == data->client_fd) {
            data->client_fd = -1;
            if (!buffer_can_read(&data->client_to_sv) && data->origin_fd == -1) {
                shutdown(data->origin_fd, SHUT_WR);
            }
        } else {
            data->origin_fd = -1;
            if (!buffer_can_read(&data->sv_to_client) && data->client_fd == -1) {
                shutdown(data->client_fd, SHUT_WR);
            }
        }

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
    fprintf(stdout, "[COPY_WRITE] key->fd: %d\n", key->fd); // Log which FD is writing
    client_data *data = ATTACHMENT(key);
    buffer *buffer_to_drain;
    int source_fd;

    if (key->fd == data->client_fd) {
        buffer_to_drain = &data->sv_to_client;
        source_fd = data->origin_fd;
        fprintf(stdout, "[COPY_WRITE] From origin_fd %d to client_fd %d\n", source_fd, key->fd);
    } else if (key->fd == data->origin_fd) {
        buffer_to_drain = &data->client_to_sv;
        source_fd = data->client_fd;
        fprintf(stdout, "[COPY_WRITE] From client_fd %d to origin_fd %d\n", source_fd, key->fd);
    } else {
        fprintf(stderr, "[COPY_WRITE] Unknown fd %d\n", key->fd);
        return SOCKS_ERROR;
    }

    size_t available_read_data;
    uint8_t *ptr = buffer_read_ptr(buffer_to_drain, &available_read_data);
    fprintf(stdout, "[COPY_WRITE] Buffer can read: %d, available data: %zu\n", buffer_can_read(buffer_to_drain), available_read_data);

    if (available_read_data == 0) {
        fprintf(stdout, "[COPY_WRITE] Buffer empty.\n");

        if (key->fd == data->origin_fd) {
            fprintf(stdout, "[COPY_WRITE] Origin_fd %d buffer empty, setting OP_READ to get response.\n", key->fd);
            selector_set_interest(key->s, key->fd, OP_READ);
        } else { fprintf(stdout, "[COPY_WRITE] Client_fd %d buffer empty, setting OP_NOOP.\n", key->fd);
            selector_set_interest(key->s, key->fd, OP_NOOP);
        }

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

        if ( (data->client_fd == -1 && !buffer_can_read(&data->client_to_sv)) &&
             (data->origin_fd == -1 && !buffer_can_read(&data->sv_to_client)) ) {
            fprintf(stdout, "[COPY_WRITE] Both sides closed and buffers empty. SOCKS_DONE\n");
            return SOCKS_DONE;
        }
        return COPY;
    }

    ssize_t n = send(key->fd, ptr, available_read_data, MSG_NOSIGNAL);
    fprintf(stdout, "[COPY_WRITE] Send returned: %zd\n", n);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fprintf(stdout, "[COPY_WRITE] EAGAIN/EWOULDBLOCK on fd %d\n", key->fd);
            return COPY;
        }
        perror("[COPY_WRITE] Send error");
        return SOCKS_ERROR;
    }
    if (n == 0) {
        fprintf(stdout, "[COPY_WRITE] Send returned 0 bytes, but data available. Possible issue on fd %d\n", key->fd);
        return SOCKS_ERROR;
    }

    buffer_read_adv(buffer_to_drain, n);
    fprintf(stdout, "[COPY_WRITE] Drained %zd bytes from buffer. Buffer can read now: %d\n", n, buffer_can_read(buffer_to_drain));

    if (source_fd != -1) {
        if (buffer_can_write(buffer_to_drain)) {
            fprintf(stdout, "[COPY_WRITE] Setting source_fd %d OP_READ (buffer has space)\n", source_fd);
            selector_set_interest(key->s, source_fd, OP_READ);
        } else {
            fprintf(stdout, "[COPY_WRITE] Setting source_fd %d OP_NOOP (buffer full)\n", source_fd);
            selector_set_interest(key->s, source_fd, OP_NOOP);
        }
    }

    if (!buffer_can_read(buffer_to_drain)) {
        fprintf(stdout, "[COPY_WRITE] Buffer empty after this send.\n");
        if (key->fd == data->origin_fd) {
            fprintf(stdout, "[COPY_WRITE] Origin_fd %d buffer now empty, setting OP_READ to get response.\n", key->fd);
            selector_set_interest(key->s, key->fd, OP_READ);
        } else {
            fprintf(stdout, "[COPY_WRITE] Client_fd %d buffer now empty, setting OP_NOOP.\n", key->fd);
            selector_set_interest(key->s, key->fd, OP_NOOP);
        }

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

        if ( (data->client_fd == -1 && !buffer_can_read(&data->client_to_sv)) &&
             (data->origin_fd == -1 && !buffer_can_read(&data->sv_to_client)) ) {
            fprintf(stdout, "[COPY_WRITE] Both sides closed and buffers empty. SOCKS_DONE\n");
            return SOCKS_DONE;
        }
    } else {
        fprintf(stdout, "[COPY_WRITE] Buffer not empty, setting %d OP_WRITE.\n", key->fd);
        selector_set_interest(key->s, key->fd, OP_WRITE);
    }

    return COPY;
}