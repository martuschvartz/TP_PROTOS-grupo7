#include <negotiation_parser.h>
#define SOCKS_VERSION 0x05

static states required_method = PASS;

int is_negotiation_read_done(neg_parser * np) {
    return np->current_state == DONE || np->current_state == ERROR;
}

int has_errors(neg_parser * np) {
    return np->current_state == ERROR;
}

void initialize_negotiation_parser(neg_parser *np) {
    if (np != NULL) {
        np->current_state = VERSION;
        np->selected_method = NOT_ACCEPTABLE;
    }
}

void negotiation_parse(neg_parser *np, buffer *buffer) {
    while (buffer_can_read(buffer) && !is_negotiation_read_done(np)) {
        switch (np->current_state) {
            case VERSION:
                negotiation_parse_version(np, buffer_read(buffer));
                break;
            case NMETHODS:
                negotiation_parse_method_count(np, buffer_read(buffer));
                break;
            case METHODS:
                negotiation_parse_methods(np, buffer_read(buffer));
                break;
            default:
                // log error
                break;
        }
    }
}

void negotiation_parse_version(neg_parser *np, uint8_t version) {
    if (SOCKS_VERSION == version) {
        np->current_state = NMETHODS;
        return;
    }
    np->current_state = ERROR;
}

void negotiation_parse_method_count(neg_parser *np, uint8_t method_quantity) {
    if (method_quantity == 0) {
        np->current_state = DONE;
        return;
    }
    np->method_quantity = method_quantity;
    np->current_state = METHODS;
}

void negotiation_parse_methods(neg_parser *np, uint8_t current_method) {
    np->method_quantity -= 1;
    if (current_method == required_method) {
        np->selected_method = current_method;
    }
    np->current_state = np->method_quantity == 0 ? DONE : NMETHODS;
}

int generate_negotiation_response(neg_parser * np, buffer * buffer) {
    if (has_errors(np) || !buffer_can_write(buffer)) {
        return 1;
    }
    buffer_write(buffer, SOCKS_VERSION);
    if (!buffer_can_write(buffer)) {
        return 1;
    }
    buffer_write(buffer, np->selected_method);
    return 0;
}