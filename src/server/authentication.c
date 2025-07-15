#include <authentication.h>

#include "users.h"

void authentication_read_init(const unsigned state, selector_key * key) {
    client_data * data = ATTACHMENT(key);
    initialize_auth_parser(&data->handshake.authentication_parser);
}

unsigned authentication_read(selector_key * key) {

    StringBuilder *sb = sb_create();
    sb_append(sb, "Authentication read started ");
    our_log(INFO, sb_get_string(sb));
    sb_free(sb);
    client_data * data = ATTACHMENT(key);
    size_t bytes_available;
    uint8_t * ptr = buffer_write_ptr(&data->client_to_sv, &bytes_available);

    if (!buffer_can_write(&data->client_to_sv)) {
        return AUTH_READ;
    }

    const ssize_t bytes_read = recv(key->fd, ptr, bytes_available, 0);

    if (bytes_read <= 0) {
        return SOCKS_ERROR;
    }

    buffer_write_adv(&data->client_to_sv, bytes_read);
    auth_parse(&data->handshake.authentication_parser, &data->client_to_sv);

    if (is_auth_done(&data->handshake.authentication_parser)) {
        int login = user_login(data->handshake.authentication_parser.uname, data->handshake.authentication_parser.passwd);
        int authenticated = login < 0 ? ACCESS_DENIED: AUTHENTICATED;
        strcpy(data->username, data->handshake.authentication_parser.uname);
        if ( auth_generate_response(&data->handshake.authentication_parser, &data->sv_to_client, authenticated) || selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS) {
            return SOCKS_ERROR;
        }
        return AUTH_WRITE;
    }
    return AUTH_READ;
}

unsigned authentication_write(selector_key * key) {
    
    StringBuilder *sb = sb_create();
    sb_append(sb, "Authentication write started ");
    our_log(INFO, sb_get_string(sb));
    sb_free(sb);
    client_data * data = ATTACHMENT(key);
    size_t bytes_to_write;
    uint8_t *ptr = buffer_read_ptr(&data->sv_to_client, &bytes_to_write);

    ssize_t bytes_sent = send(key->fd, ptr, bytes_to_write, MSG_NOSIGNAL);
    if (bytes_sent <= 0) {
        return SOCKS_ERROR;
    }

    buffer_read_adv(&data->sv_to_client, bytes_sent);
    if (buffer_can_read(&data->sv_to_client)) {
        return AUTH_WRITE;
    }

    if (selector_set_interest_key(key, OP_READ) != SELECTOR_SUCCESS) {
        return SOCKS_ERROR;
    }

    return REQ_READ;
}