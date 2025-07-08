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
    METHODS
    //DONE,
    //ERROR
}states;

typedef struct neg_parser {
    method_types selected_method;
    states current_state;
    uint8_t method_quantity;
}neg_parser;

/**
 * Checks if negotiation has ended
 * @param np negotiation parser
 * @return !=0 if true, else 0
 */
int is_negotiation_read_done(neg_parser * np);

/**
 * Checks if negotiation has errors
 * @param np negotiation parser
 * @return !=0 if true, else 0
 */
int has_errors(neg_parser * np);

/**
 * Initializes parser
 * @param np negotiation parser
 */
void initialize_negotiation_parser(neg_parser * np);

/**
 * Reads from buffer and processes version, nmethods, and methods
 * @param np negotiation parser
 * @buffer from which to read characters
 */
void negotiation_parse(neg_parser * np, buffer * buffer);

/**
 * Checks valid version
 * @param np negotiation parser
 * @version version
 */
void negotiation_parse_version(neg_parser * np, uint8_t version);

/**
 * Checks method count
 * @param np negotiation parser
 * @method_quantity method count
 */
void negotiation_parse_method_count(neg_parser * np, uint8_t method_quantity);

/**
 * Checks valid method
 * @param np negotiation parser
 * @version version
 */
void negotiation_parse_methods(neg_parser * np, uint8_t current_method);

/**
 * Leaves response in buffer
 * @param np negotiation parser
 * @buffer buffer to leave response
 * @return 0 if successful, else 1
 */
int generate_negotiation_response(neg_parser * np, buffer * buffer);

#endif
