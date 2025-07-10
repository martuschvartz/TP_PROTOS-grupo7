#ifndef SOCKS5_H
#define SOCKS5_H

#include <selector.h>
#include <buffer.h>
#include <defaults.h>
#include <stm.h>
#include <negotiation_parser.h>
#include "metrics.h"

/** obtiene el struct (client_data *) desde la llave de selección  */
#define ATTACHMENT(key) ( (struct client_data *)(key)->data)

/* Estados generales para la máquina de estados */
enum socks_v5state {
    /**
     * Recibe el mensaje de negociación y lo procesa
     *
     * Intereses:
     *     - OP_READ sobre client_fd
     *
     * Transiciones:
     *   - NEGOTIATION_READ     mientras el mensaje no esté completo
     *   - NEGOTIATION_WRITE    cuando está completo
     *   - ERROR                ante cualquier error (IO/parseo)
     */
    NEGOTIATION_READ=0,

    /**
     * Envía la respuesta de la negociación al cliente.
     *
     * Intereses:
     *     - OP_WRITE sobre client_fd
     *
     * Transiciones:
     *   - NEGOTIATION_WRITE    mientras queden bytes por enviar
     *   - AUTH_READ            cuando se enviaron todos los bytes
     *   - ERROR                ante cualquier error (IO/parseo)
     */
    NEGOTIATION_WRITE,

    // /**
    //  * Lee y procesa usuario/contraseña del cliente
    //  *
    //  * Intereses:
    //  *     - OP_READ sobre client_fd
    //  *
    //  * Transiciones:
    //  *   - AUTH_READ        mientras el mensaje no esté completo
    //  *   - AUTH_WRITE       cuando está completo
    //  *   - ERROR            ante cualquier error (IO/parseo)
    //  */
    // AUTH_READ,
    //
    // /**
    //  * Envía la respuesta de la autenticación al cliente.
    //  *
    //  * Intereses:
    //  *     - OP_WRITE sobre client_fd
    //  *
    //  * Transiciones:
    //  *   - AUTH_WRITE       mientras queden bytes por enviar
    //  *   - REQ_READ         cuando se enviaron todos los bytes
    //  *   - ERROR            ante cualquier error (IO/parseo)
    //  */
    // AUTH_WRITE,
    //
    // /**
    //  * Lee y procesa la request del cliente
    //  *
    //  * Intereses:
    //  *     - OP_READ sobre client_fd
    //  *
    //  * Transiciones:
    //  *   - REQ_READ         mientras el mensaje no esté completo
    //  *   - REQ_RESOLVE      cuando está completo y se precise resolver un nombre de dominio
    //  *   - REQ_CONNECT      cuando está completo y no se precisa resolver un nombre de dominio
    //  *   - REQ_WRITE        ante cualquier problema de la request
    //  *   - ERROR            ante cualquier error (IO/parseo)
    //  */
    // REQ_READ,
    //
    // /**
    //  * Resuelve el nombre de dominio
    //  *
    //  * Intereses:
    //  *     - OP_NOOP sobre client_fd
    //  *
    //  * Transiciones:
    //  *   - REQ_CONNECT      si se pudo resolver exitosamente
    //  *   - REQ_WRITE        cuando está completo
    //  */
    // REQ_RESOLVE,
    //
    // /**
    //  *  Realiza la conección con el origin
    //  *
    //  *  Intereses:
    //  *      - OP_WRITE sobre client_fd
    //  *
    //  *  Transiciones:
    //  *    - REQ_WRITE       cuando la conexión fue establecida
    //  */
    // REQ_CONNECT,
    //
    // /**
    //  *  Envía la respuesta de la conexión al cliente
    //  *
    //  *  Intereses:
    //  *
    //  */
    // REQ_WRITE,
    //
    // SOCKS5_READ,
    //
    // SOCKS5_WRITE,
    //
    // SOCKS5_BLOCK,
    //
    // SOCKS5_CLOSE,

    // for testing purposes
    ECHO_READ,

    ECHO_WRITE,

    // estados terminales
    SOCKS_DONE,
    SOCKS_ERROR,
};


// test purposes, doesn´t need parser
typedef struct echo_st{
    buffer bf;
    uint8_t bf_raw[BUFFER_SIZE];
} echo_st;

typedef struct negotiation_st {
    buffer bf;
    uint8_t bf_raw[BUFFER_SIZE];
    neg_parser parser;
} negotiation_st;

typedef struct client_data{
    struct state_machine stm;

    // estados para el client_fd
    union{
        echo_st echo;
        negotiation_st negotiation;
    } client;

    int client_fd;

    // estados para el origin_fd

}client_data;

void socks_v5_passive_accept(selector_key * selector_key);

#endif