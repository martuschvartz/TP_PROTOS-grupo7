#ifndef REQUEST_PARSER_H
#define REQUEST_PARSER_H

#include <stdint.h>
#include <netinet/in.h>

#include "buffer.h"

#define MAX_ADDR_LENGTH 255

typedef enum req_states {
    REQ_VERSION=0,
    REQ_CMD,
    REQ_RSV,
    REQ_ATYP,
    REQ_DST_ADDR_LENGTH,
    REQ_DST_ADDR,
    REQ_DST_PORT,
    REQ_DONE,
    REQ_ERROR,
}req_states;

typedef enum connection_status {
    CON_SUCCEEDED=0,
    CON_SERVER_FAILURE,
    CON_NOT_ALLOWED,
    CON_NET_UNREACHABLE,
    CON_HOST_UNREACHABLE,
    CON_REFUSED,
    CON_TTL_EXPIRED,
    CON_COMMAND_NOT_SUPPORTED,
    CON_ATYPE_NOT_SUPPORTED,
    CON_OTHER,
}connection_status;

typedef enum req_atyp {
    IPV4 = 0x01,
    DN = 0x03,
    IPV6 = 0x04,
}req_atyp;


typedef struct address {
    // todo for logs
    union{
        struct in_addr ipv4;
        struct in6_addr ipv6;
        uint8_t dn[MAX_ADDR_LENGTH + 1];
    }address_class;

    uint8_t buffer[MAX_ADDR_LENGTH + 1];
}address;

typedef struct req_parser {
    req_states current_state;
    connection_status connection_status;

    req_atyp atyp;
    uint8_t addr_len;
    uint8_t bytes_read;

    in_port_t port;

    address address;
} req_parser;

/**
 * initializes parser
 * @param rp request parser
 */
void initialize_request_parser(req_parser * rp);


/**
 * Checks protocol version
 * @param rp request parser
 * @param version protocol version (0x05)
 */
void req_parse_version(req_parser * rp, uint8_t version);

/**
 * Checks command (CONNECT)
 * @param rp request parser
 * @param cmd command
 */
void req_parse_cmd(req_parser * rp, uint8_t cmd);

/**
 * Checks for reserved byte
 * @param rp request parser
 * @param rsv reserved byte
 */
void req_parse_rsv(req_parser * rp, uint8_t rsv);

/**
 * Checks address type of dst addr
 * @param rp request parser
 * @param atyp address type
 */
void req_parse_atyp(req_parser * rp, uint8_t atyp);

/**
 * Checks dn length
 * @param rp request parser
 * @param dn_length domain name length
 */
void req_parse_dst_addr_length(req_parser *rp, uint8_t dn_length);

/**
 * Reads dst address and stores it
 * @param rp request parser
 * @param buffer buffer that contains address
 */
void req_parse_dst_addr(req_parser * rp, uint8_t byte);

/**
 * Reads dst port
 * @param rp request parser
 * @param buffer buffer that contains the port
 */
void req_parse_dst_port(req_parser * rp, uint8_t byte);

/**
 * Checks if there are errors in the request phase
 * @param rp request parser
 * @return != 0 if true, else 0
 */
int req_has_error(req_parser * rp);

/**
 * Checks if request phase has ended
 * @param rp request parser
 * @return != if true, else 0
 */
int is_req_done(req_parser * rp);

/**
 *
 * @param rp request parser
 * @param buffer where the response will be stored
 * @return 0 on success, 1 on error
 */
int req_generate_response(req_parser * rp, buffer * buffer);


/**
 * Parses user request for connection
 * @param rp request parser
 * @param buffer buffer from which to read user request
 */
void req_parse(req_parser * rp, buffer * buffer);

#endif //REQUEST_PARSER_H
