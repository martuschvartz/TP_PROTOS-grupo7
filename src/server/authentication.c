#include <authentication.h>

void authentication_read_init(const unsigned state, selector_key * key) {
    client_data * data = ATTACHMENT(key);
    initialize_auth_parser(&data->client.authentication.parser);
}

unsigned authentication_read(selector_key * key) {
    fprintf(stdout, "Authentication Read Started\n");
    client_data * data = ATTACHMENT(key);
    size_t bytes_available;
    uint8_t * ptr = buffer_write_ptr(&data->client.authentication.bf, &bytes_available);

    if (!buffer_can_write(&data->client.authentication.bf)) {
        return AUTH_READ;
    }

    const ssize_t bytes_read = recv(key->fd, ptr, bytes_available, 0);

    if (bytes_read <= 0) {
        return SOCKS_ERROR;
    }

    buffer_write_adv(&data->client.authentication.bf, bytes_read);
    auth_parse(&data->client.authentication.parser, &data->client.authentication.bf);

    if (is_auth_done(&data->client.authentication.parser)) {
        if ( auth_generate_response(&data->client.authentication.parser, &data->client.negotiation.bf) ||   selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS) {
            return SOCKS_ERROR;
        }
        return AUTH_WRITE;
    }
    return AUTH_READ;
}

unsigned authentication_write(selector_key * key) {
    fprintf(stdout, "Authentication Write Started\n");
    client_data * data = ATTACHMENT(key);
    size_t bytes_to_write;
    uint8_t *ptr = buffer_write_ptr(&data->client.authentication.bf, &bytes_to_write);

    ssize_t bytes_sent = send(key->fd, ptr, bytes_to_write, MSG_NOSIGNAL);
    if (bytes_sent <= 0) {
        return SOCKS_ERROR;
    }

    buffer_read_adv(&data->client.authentication.bf, bytes_sent);
    if (buffer_can_read(&data->client.authentication.bf)) {
        return AUTH_WRITE;
    }

    if (selector_set_interest_key(key, OP_READ) != SELECTOR_SUCCESS) {
        return SOCKS_ERROR;
    }

    return ECHO_READ;
}