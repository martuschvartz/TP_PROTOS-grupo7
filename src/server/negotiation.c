#include <negotiation.h>

void negotiation_read_init(const unsigned state, selector_key *key) {
    //inicializa el parser
    client_data * client_data = ATTACHMENT(key);
    initialize_negotiation_parser(&client_data->client.negotiation.parser);
}

unsigned int negotiation_read(selector_key *key) {
    fprintf(stdout, "Negotiation Read Started\n");
    client_data *client_data = ATTACHMENT(key);
    size_t bytes_available;
    uint8_t *ptr = buffer_write_ptr(&client_data->client.negotiation.bf, &bytes_available);

    // wait? buffer is full
    if (!buffer_can_write(&client_data->client.echo.bf)) {
        return NEGOTIATION_READ;
    }

    const ssize_t read_count = recv(key->fd, ptr, bytes_available, 0);
    if (read_count <= 0) {
        return SOCKS_ERROR;
    }

    buffer_write_adv(&client_data->client.negotiation.bf, read_count);
    negotiation_parse(&client_data->client.negotiation.parser, &client_data->client.negotiation.bf);
    if (is_negotiation_read_done(&client_data->client.negotiation.parser)) {
        if (selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS || generate_negotiation_response(&client_data->client.negotiation.parser, &client_data->client.negotiation.bf)) {
            return SOCKS_ERROR;
        }
        return NEGOTIATION_WRITE;
    }
    return NEGOTIATION_READ;
}

unsigned int negotiation_write(selector_key *key) {
    fprintf(stdout, "Negotiation Write Started\n");
    client_data *client_data = ATTACHMENT(key);
    size_t bytes_to_write;
    uint8_t *ptr = buffer_read_ptr(&client_data->client.negotiation.bf, &bytes_to_write);

    ssize_t bytes_sent = send(key->fd, ptr, bytes_to_write, MSG_NOSIGNAL);
    if (bytes_sent <= 0) {
        return SOCKS_ERROR;
    }

    moreBytes((unsigned int)bytes_sent);
    
    buffer_read_adv(&client_data->client.negotiation.bf, bytes_sent);
    // there are bytes to be sent yet
    if (buffer_can_read(&client_data->client.negotiation.bf)) {
        return NEGOTIATION_WRITE;
    }

    if (selector_set_interest_key(key, OP_READ) != SELECTOR_SUCCESS) {
        return SOCKS_ERROR;
    }
    return  ECHO_READ;
}
