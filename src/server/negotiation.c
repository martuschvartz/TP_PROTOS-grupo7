#include <negotiation.h>

void negotiation_read_init(const unsigned state, selector_key *key) {
    //inicializa el parser
    client_data * client_data = ATTACHMENT(key);
    initialize_negotiation_parser(&client_data->handshake.negotiation_parser);
}

unsigned int negotiation_read(selector_key *key) {

    StringBuilder *sb = sb_create();
    sb_append(sb, "Negotiation read started");
    our_log(INFO, sb_get_string(sb));
    sb_free(sb);
    client_data *client_data = ATTACHMENT(key);
    size_t bytes_available;
    uint8_t *ptr = buffer_write_ptr(&client_data->client_to_sv, &bytes_available);

    // wait? buffer is full
    if (!buffer_can_write(&client_data->client_to_sv)) {
        return NEGOTIATION_READ;
    }

    const ssize_t read_count = recv(key->fd, ptr, bytes_available, 0);
    if (read_count <= 0) {
        return SOCKS_ERROR;
    }

    buffer_write_adv(&client_data->client_to_sv, read_count);
    negotiation_parse(&client_data->handshake.negotiation_parser, &client_data->client_to_sv);
    if (is_negotiation_read_done(&client_data->handshake.negotiation_parser)) {
        if (selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS || generate_negotiation_response(&client_data->handshake.negotiation_parser, &client_data->sv_to_client)) {
            return SOCKS_ERROR;
        }
        return NEGOTIATION_WRITE;
    }
    return NEGOTIATION_READ;
}

unsigned int negotiation_write(selector_key *key) {

    StringBuilder *sb = sb_create();
    sb_append(sb, "Negotiation write started");
    our_log(INFO, sb_get_string(sb));
    sb_free(sb);
    client_data *client_data = ATTACHMENT(key);
    size_t bytes_to_write;
    uint8_t *ptr = buffer_read_ptr(&client_data->sv_to_client, &bytes_to_write);

    ssize_t bytes_sent = send(key->fd, ptr, bytes_to_write, MSG_NOSIGNAL);
    if (bytes_sent <= 0) {
        return SOCKS_ERROR;
    }

    more_bytes((long)bytes_sent);
    
    buffer_read_adv(&client_data->sv_to_client, bytes_sent);
    // there are bytes to be sent yet
    if (buffer_can_read(&client_data->sv_to_client)) {
        return NEGOTIATION_WRITE;
    }

    if (client_data->handshake.negotiation_parser.selected_method == NOT_ACCEPTABLE || selector_set_interest_key(key, OP_READ) != SELECTOR_SUCCESS) {
        return SOCKS_ERROR;
    }
    return  AUTH_READ;
}
