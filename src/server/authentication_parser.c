#include <authentication_parser.h>
#include <stddef.h>
#include <stdint.h>
#include <users.h>
#include <negotiation_parser.h>

#define AUTH_VERSION 0x01

void initialize_auth_parser(auth_parser * ap) {
    if (ap != NULL) {
        ap->current_state = AUTH_PARSE_VERSION;
        ap->authenticated = ACCESS_DENIED;
        ap->length = 0;
        ap->bytes_read = 0;
    }
}


void auth_parse_version(auth_parser * ap, uint8_t version) {
    if (version == AUTH_VERSION) {
        ap->current_state = AUTH_ULEN;
        return;
    }
    ap->current_state = AUTH_ERROR;
}


void auth_parse_length(auth_parser * ap, uint8_t length) {
    if (length > MAX_LENGTH) {
        ap->current_state = AUTH_ERROR;
        return;
    }
    if (length == 0 && ap->current_state == AUTH_ULEN) {
        ap->current_state = AUTH_PLEN;
        return;
    }
    ap->current_state = (ap->current_state == AUTH_ULEN) ? AUTH_UNAME : AUTH_PASSWD;
    ap->length = length;
}


void auth_parse_characters(auth_parser * ap, uint8_t byte) {
    if (ap->current_state == AUTH_UNAME) {
        ap->uname[ap->bytes_read++] = byte;
    }else {
        ap->passwd[ap->bytes_read++] = byte;
    }

    if (ap->bytes_read == ap->length) {
        ap->bytes_read = 0;
        ap->current_state = (ap->current_state == AUTH_UNAME) ? AUTH_PLEN : AUTH_DONE;
    }
}


int auth_has_error(auth_parser * ap) {
    return ap->current_state == AUTH_ERROR;
}


int is_auth_done(auth_parser * ap) {
    return ap->current_state == AUTH_DONE || ap->current_state == AUTH_ERROR;
}


int auth_generate_response(auth_parser * ap, buffer * buffer, int authenticated) {
    if (!buffer_can_write(buffer)) {
        return 1;
    }
    buffer_write(buffer, AUTH_VERSION);
    if (!buffer_can_write(buffer)) {
        return 1;
    }
    buffer_write(buffer, authenticated);
    return 0;
}

void auth_parse(auth_parser * ap, buffer * buffer) {
    while (buffer_can_read(buffer) && !is_auth_done(ap)) {
        if (ap->current_state == AUTH_PARSE_VERSION) {
            auth_parse_version(ap, buffer_read(buffer));
        }
        else if (ap->current_state == AUTH_ULEN || ap->current_state == AUTH_PLEN) {
            auth_parse_length(ap, buffer_read(buffer));
        }
        else if (ap->current_state == AUTH_UNAME || ap->current_state == AUTH_PASSWD) {
            auth_parse_characters(ap, buffer_read(buffer));
        }
        else {
            return;
        }
    }
}