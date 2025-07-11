#include <authentication_parser.h>
#include <stddef.h>
#include <stdint.h>
#include <users.h>

#include "negotiation_parser.h"

// rfc says 0x01 -> gssapi?
#define AUTH_VERSION 0x02

void initialize_auth_parser(auth_parser * ap) {
    if (ap != NULL) {
        ap->current_state = AUTH_VERSION;
        ap->authenticated = 0;
        ap->length = 0;
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


void auth_parse_characters(auth_parser * ap, buffer * buffer) {
    int i  = 0;
    while(i < ap->length && buffer_can_read(buffer)) {
        ap->uname[i++] = buffer_read(buffer);
    }
    ap->length = 0;
    ap->current_state = (ap->current_state == AUTH_UNAME) ? AUTH_PLEN : AUTH_DONE;
}


int auth_has_error(auth_parser * ap) {
    return ap->current_state == AUTH_ERROR;
}


int is_auth_done(auth_parser * ap) {
    return ap->current_state == AUTH_DONE || ap->current_state == AUTH_ERROR;
}


int auth_generate_response(auth_parser * ap, buffer * buffer) {
    int login = userLogin(ap->uname, ap->passwd);
    ap->authenticated = login < 0 ? ACCESS_DENIED: AUTHENTICATED;
    if (!buffer_can_write(buffer)) {
        return 1;
    }
    buffer_write(buffer, AUTH_VERSION);
    if (!buffer_can_write(buffer)) {
        return 1;
    }
    buffer_write(buffer, ap->authenticated);
    return 0;
}

void auth_parse(auth_parser * ap, buffer * buffer) {
    while (buffer_can_read(buffer) && !is_auth_done(ap)) {
        if (ap->current_state == AUTH_VERSION) {
            auth_parse_version(ap, buffer_read(buffer));
        }
        else if (ap->current_state == AUTH_ULEN || ap->current_state == AUTH_PLEN) {
            auth_parse_length(ap, buffer_read(buffer));
        }
        else if (ap->current_state == AUTH_UNAME || ap->current_state == AUTH_PASSWD) {
            auth_parse_characters(ap, buffer);
        }
        else {
            // log error
            return;
        }
    }
}