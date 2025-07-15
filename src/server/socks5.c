#include <socks5.h>
#include <sys/socket.h>
#include <selector.h>
#include <stdlib.h>
#include <copy.h>
#include <stdio.h>
#include <negotiation.h>
#include <authentication.h>

#include "request.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

void done_arrival(const unsigned state, selector_key * key) {
    StringBuilder *sb = sb_create();
    sb_append(sb, "Done state");
    our_log(INFO, sb_get_string(sb));
    sb_free(sb);
}

void error_arrival(const unsigned state, selector_key * key) {
      StringBuilder *sb = sb_create();
    sb_append(sb, "Error state");
    our_log(ERROR, sb_get_string(sb));
    sb_free(sb);
}

/** definición de handlers para cada estado */
static const struct state_definition client_actions[] = {
    {
        .state              = NEGOTIATION_READ,
        .on_arrival         = negotiation_read_init,
        .on_read_ready      = negotiation_read,
    },
    {
        .state              = NEGOTIATION_WRITE,
        .on_write_ready      = negotiation_write,
    },
    {
        .state              = AUTH_READ,
        .on_arrival         = authentication_read_init,
        .on_read_ready      = authentication_read,
    },
    {
        .state              = AUTH_WRITE,
        .on_write_ready     = authentication_write,
    },
    {
        .state              = REQ_READ,
        .on_arrival         = request_read_init,
        .on_read_ready      = request_read,
    },
    {
        .state              = REQ_RESOLVE,
        .on_block_ready     = request_resolve,
    },
    {
        .state              = REQ_CONNECT,
        .on_write_ready     = request_connect,
    },
    {
        .state              = REQ_WRITE,
        .on_write_ready     = request_write,
    },
    {
        .state              = COPY,
        .on_read_ready      = copy_read,
        .on_write_ready     = copy_write,
    },
    {
        .state              = SOCKS_DONE,
        .on_arrival         = done_arrival
    },
    {
        .state              = SOCKS_ERROR,
        .on_arrival         = error_arrival
    }
};

// Handlers top level de la conexión pasiva.
// son los que emiten los eventos a la maquina de estados.
static void socksv5_done(struct selector_key* key);

static void socksv5_destroy(client_data* data) {
    if (data == NULL) {
        return;
    }
    if (data->origin_addr != NULL) {
        if (data->handshake.request_parser.atyp != DN) {
            free(data->origin_addr->ai_addr);
            free(data->origin_addr);
        }else {
            freeaddrinfo(data->origin_addr);
        }
        data->origin_addr = NULL;
        data->current_origin_addr = NULL;
    }
    free(data);
}

static void socksv5_read(selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum socks_v5state st = stm_handler_read(stm, key);

    if(SOCKS_ERROR == st || SOCKS_DONE == st) {
        socksv5_done(key);
    }
}

static void socksv5_write(selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum socks_v5state st = stm_handler_write(stm, key);

    if(SOCKS_ERROR == st || SOCKS_DONE == st) {
        socksv5_done(key);
    }
}

static void socksv5_block(selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum socks_v5state st = stm_handler_block(stm, key);

    if(SOCKS_ERROR == st || SOCKS_DONE == st) {
        socksv5_done(key);
    }
}

static void socksv5_close(selector_key *key) {
    client_data * client_data =ATTACHMENT(key);
    struct state_machine *stm   = &client_data->stm;
    stm_handler_close(stm, key);


    close(key->fd);
    if (key->fd == client_data->client_fd) {
        client_data->client_fd = -1;
    }
    else if (key->fd == client_data->origin_fd) {
        client_data->origin_fd = -1;
    }

    if (client_data->origin_fd == -1 && client_data->client_fd == -1) {
        socksv5_destroy(ATTACHMENT(key));
        less_connections(); //TODO puede ser q vaya en la funcion de socksv5_done
    }
}

//chequear
static void socksv5_done(selector_key* key) {
    const int fds[] = {
        ATTACHMENT(key)->client_fd,
        ATTACHMENT(key)->origin_fd,
    };
    for(unsigned i = 0; i < N(fds); i++) {
        if(fds[i] != -1) {
            if(SELECTOR_SUCCESS != selector_unregister_fd(key->s, fds[i])) {
                abort();
            }
        }
    }
}


static client_data * socksv5_new(int client_fd){
    client_data * new_client = calloc(1, sizeof(struct client_data));
    
    if(new_client != NULL){
        new_client->stm.initial = NEGOTIATION_READ;
        new_client->stm.max_state = SOCKS_ERROR;
        new_client->stm.states = client_actions;
        new_client->client_fd = client_fd;
        new_client->client_eof = 0;
        new_client->origin_eof = 0;
        new_client->origin_fd = -1;
        buffer_init(&new_client->client_to_sv, BUFFER_SIZE, new_client->client_to_sv_raw);
        buffer_init(&new_client->sv_to_client, BUFFER_SIZE, new_client->sv_to_client_raw);
        stm_init(&new_client->stm);
    }

    return new_client;
}

static const fd_handler socks5_handler = {
    .handle_read   = socksv5_read,
    .handle_write  = socksv5_write,
    .handle_close  = socksv5_close,
    .handle_block  = socksv5_block,
};

// check
fd_handler * get_socks5_handlers() {
    return &socks5_handler;
}

void socks_v5_passive_accept(selector_key * selector_key){
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    client_data *state = NULL;

    const int client = accept(selector_key->fd, (struct sockaddr *)&client_addr,
                              &client_addr_len);
    if (client == -1)
    {
        return;
    }
    if (selector_fd_set_nio(client) == -1)
    {
        return;
    }

    state = socksv5_new(client);
    if(state == NULL){
        return;
    }

    if (SELECTOR_SUCCESS != selector_register(selector_key->s, client, &socks5_handler, OP_READ, state)){
        goto fail;
    }
    
    more_connections();
    return;

fail: 
    if(client != -1){
        close(client);
    }
    socksv5_destroy(state);
}