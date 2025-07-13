#include <errno.h>
#include <request.h>
#include <request_parser.h>
#include <socks5.h>
#include <stdio.h>

void request_read_init(const unsigned state, selector_key * key) {
    client_data * data = ATTACHMENT(key);
    initialize_request_parser(&data->handshake.request_parser);
}

unsigned request_read(selector_key * key) {
    fprintf(stdout, "Request Read Started\n");
    client_data * data = ATTACHMENT(key);
    size_t bytes_available;
    uint8_t * ptr = buffer_write_ptr(&data->client_to_sv, &bytes_available);

    if (!buffer_can_write(&data->client_to_sv)) {
        return REQ_READ;
    }

    const ssize_t bytes_read = recv(key->fd, ptr, bytes_available, 0);

    if (bytes_read == 0) {
        fprintf(stderr, "Client closed connection\n");
        return SOCKS_ERROR;
    }

    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return REQ_READ;
        }
        return SOCKS_ERROR;
    }

    buffer_write_adv(&data->client_to_sv, bytes_read);
    req_parse(&data->handshake.request_parser, &data->client_to_sv);

    if (is_req_done(&data->handshake.request_parser)) {
        if (req_has_error(&data->handshake.request_parser)) {
            if (req_generate_response(&data->handshake.request_parser, &data->sv_to_client) || selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS) {
                return SOCKS_ERROR;
            }
            return REQ_WRITE;
        }
        if (data->handshake.request_parser.atyp == DN) {
            selector_set_interest_key(key, OP_NOOP);
            return REQ_RESOLVE;
        }
        selector_set_interest_key(key, OP_WRITE);
        return REQ_CONNECT;
    }
    return REQ_READ;
}

unsigned request_write(selector_key * key) {
    fprintf(stdout, "Request Write Started\n");
    fprintf(stdout, "Authentication Write Started\n");
    client_data * data = ATTACHMENT(key);
    size_t bytes_to_write;
    uint8_t *ptr = buffer_read_ptr(&data->sv_to_client, &bytes_to_write);

    ssize_t bytes_sent = send(key->fd, ptr, bytes_to_write, MSG_NOSIGNAL);

    if (bytes_sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return REQ_WRITE;
        return SOCKS_ERROR;
    }

    if (bytes_sent == 0) {
        return SOCKS_ERROR;
    }

    buffer_read_adv(&data->sv_to_client, bytes_sent);
    if (buffer_can_read(&data->sv_to_client)) {
        return REQ_WRITE;
    }

    if (selector_set_interest_key(key, OP_READ) != SELECTOR_SUCCESS) {
        return SOCKS_ERROR;
    }

    return SOCKS_DONE;
}