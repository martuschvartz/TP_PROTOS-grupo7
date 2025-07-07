#ifndef NEGOTIATION_PARSER_H
#define NEGOTIATION_PARSER_H

#include <buffer.h>

typedef enum method_types {
    PASS = 0x2,
    NOT_ACCEPTABLE = 0xFF
}method_types;

typedef enum states {
    VERSION = 0,
    NMETHODS,
    METHODS,
    DONE,
    ERROR
}states;

typedef struct neg_parser {
    method_types selected_method;
    states current_state;
    uint8_t method_quantity;
}neg_parser;

void initialize_negotiation_parser(neg_parser * np);

void negotiation_parse(neg_parser * p, buffer * buffer);

void negotiation_parse_version(neg_parser * p, uint8_t version);

void negotiation_parse_method_count(neg_parser * p, uint8_t method_quantity);

void negotiation_parse_methods(neg_parser * p, uint8_t current_method);

#endif
