#ifndef AUTHENTICATION_PARSER_H
#define AUTHENTICATION_PARSER_H

/**
* Once the SOCKS V5 server has started, and the client has selected the
   Username/Password Authentication protocol, the Username/Password
   subnegotiation begins.  This begins with the client producing a
   Username/Password request:

           +----+------+----------+------+----------+
           |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
           +----+------+----------+------+----------+
           | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
           +----+------+----------+------+----------+

   The VER field contains the current version of the subnegotiation,
   which is X'01'. The ULEN field contains the length of the UNAME field
   that follows. The UNAME field contains the username as known to the
   source operating system. The PLEN field contains the length of the
   PASSWD field that follows. The PASSWD field contains the password
   association with the given UNAME.

   The server verifies the supplied UNAME and PASSWD, and sends the
   following response:

                        +----+--------+
                        |VER | STATUS |
                        +----+--------+
                        | 1  |   1    |
                        +----+--------+

   A STATUS field of X'00' indicates success. If the server returns a
   `failure' (STATUS value other than X'00') status, it MUST close the
   connection.
 */


#include <stdint.h>
#include <buffer.h>


#define MAX_LENGTH 255

typedef enum auth_states{
    AUTH_PARSE_VERSION = 0,
    AUTH_ULEN,
    AUTH_UNAME,
    AUTH_PLEN,
    AUTH_PASSWD,
    AUTH_DONE,
    AUTH_ERROR,
}auth_states;

typedef enum auth_status {
    AUTHENTICATED = 0x00,
    ACCESS_DENIED,
}auth_status;

typedef struct auth_parser {
    auth_states current_state;
    auth_status authenticated;

    uint8_t length;
    uint8_t bytes_read;

    char uname[MAX_LENGTH + 1];
    char passwd[MAX_LENGTH + 1];
}auth_parser;

/**
 * Initializes provided parser
 * @param ap auth parser
 */
void initialize_auth_parser(auth_parser * ap);

/**
 * Checks valid version
 * @param ap auth parser
 * @param version provided version from user
 */
void auth_parse_version(auth_parser * ap, uint8_t version);

/**
 * Checks for uname and passwd length
 * @param ap auth parser
 * @param length uname/passwd length
 */
void auth_parse_length(auth_parser * ap, uint8_t length);

/**
 * Parses and validates username/password
 * @param ap auth parser
 * @param byte next byte to parse
 */
void auth_parse_characters(auth_parser * ap, uint8_t byte);

/**
 * Checks if the authentication sub-negotiation failed
 * @param ap auth parser
 * @return != 0 if true, else 0
 */
int auth_has_error(auth_parser * ap);

/**
 * Checks if authentication sub-negotiation has ended
 * @param ap auth parser
 * @return != 0 if true, else 0
 */
int is_auth_done(auth_parser * ap);

/**
 * Generates server response
 * @param ap auth parser
 * @param buffer where the response is stored
 * @param authenticated if the user could log in successfully
 * @return 0 if successful, 1 on error
 */
int auth_generate_response(auth_parser * ap, buffer * buffer, int authenticated);

/**
 * Parses user request for authentication
 * @param ap auth parser
 * @param buffer buffer from which to read user request
 */
void auth_parse(auth_parser * ap, buffer * buffer);

#endif
