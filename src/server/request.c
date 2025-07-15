#include <errno.h>
#include <pthread.h>
#include <request.h>
#include <request_parser.h>
#include <socks5.h>
#include <stdio.h>
#include <string.h>
#include "logger.h"
#include <sys/socket.h>
#include <arpa/inet.h> // For getsockname

static void* request_resolve_domain_name(void* arg);
static unsigned initiate_origin_connection(selector_key * key);
static unsigned req_generate_response_error(selector_key * key, connection_status msg);


void request_read_init(const unsigned state, selector_key * key) {
    client_data * data = ATTACHMENT(key);
    initialize_request_parser(&data->handshake.request_parser);
}

static unsigned req_generate_response_error(selector_key * key, connection_status msg) {
    req_parser * rp = &ATTACHMENT(key)->handshake.request_parser;
    rp->current_state = REQ_ERROR;
    rp->connection_status = msg;

    struct sockaddr_storage dummy_bnd_addr = {0};
    dummy_bnd_addr.ss_family = AF_INET;

    if (selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS || req_generate_response(rp, &ATTACHMENT(key)->sv_to_client, &dummy_bnd_addr, sizeof(dummy_bnd_addr))) {
        return SOCKS_ERROR;
    }
    return REQ_WRITE;
}

static unsigned handle_request(selector_key * key) {
    client_data * data = ATTACHMENT(key);
    req_parser rp = data->handshake.request_parser;
    uint8_t atyp = rp.atyp;

    if (atyp == IPV4) {
        struct sockaddr_in * sockaddr = malloc(sizeof(struct sockaddr_in));
        if (sockaddr == NULL) {
            our_log(ERROR, "Failed to allocate IPv4 address struct");
            return req_generate_response_error(key, CON_SERVER_FAILURE);
        }
        data->origin_addr = calloc(1, sizeof(struct addrinfo));
        if (data->origin_addr == NULL) {
            free(sockaddr);
            our_log(ERROR, "Failed to allocate server address struct");
            return req_generate_response_error(key, CON_SERVER_FAILURE);
        }
        sockaddr->sin_family = AF_INET;
        sockaddr->sin_addr = rp.address.address_class.ipv4;
        sockaddr->sin_port = htons(rp.port);

        data->origin_addr->ai_family = AF_INET;
        data->origin_addr->ai_socktype = SOCK_STREAM;
        data->origin_addr->ai_addr = (struct sockaddr *)sockaddr;
        data->origin_addr->ai_addrlen = sizeof(*sockaddr);

        data->current_origin_addr = data->origin_addr;
        return initiate_origin_connection(key);
    }
    if (atyp == IPV6) {
        struct sockaddr_in6 * sockaddr = malloc(sizeof(struct sockaddr_in6));
        if (sockaddr == NULL) {
            our_log(ERROR, "Failed to allocate IPv6 address struct");
            return req_generate_response_error(key, CON_SERVER_FAILURE);
        }
        data->origin_addr = calloc(1, sizeof(struct addrinfo));
        if (data->origin_addr == NULL) {
            free(sockaddr);
            our_log(ERROR, "Failed to allocate server address struct");
            return req_generate_response_error(key, CON_SERVER_FAILURE);
        }
        sockaddr->sin6_family = AF_INET6;
        sockaddr->sin6_addr = rp.address.address_class.ipv6;
        sockaddr->sin6_port = htons(rp.port);

        data->origin_addr->ai_family = AF_INET6;
        data->origin_addr->ai_socktype = SOCK_STREAM;
        data->origin_addr->ai_addr = (struct sockaddr *)sockaddr;
        data->origin_addr->ai_addrlen = sizeof(*sockaddr);

        data->current_origin_addr = data->origin_addr;

        return initiate_origin_connection(key);
    }
    pthread_t thread_id;
    resolve_job * job = malloc(sizeof(resolve_job));
    if (job == NULL) {
        our_log(ERROR, "Failed to allocate job struct");
        return req_generate_response_error(key, CON_SERVER_FAILURE);
    }
    job->selector = key->s;
    job->client_fd = key->fd;
    job->client_data = data;

    if (pthread_create(&thread_id, NULL, request_resolve_domain_name, job) == -1) {
        free(job);
        our_log(ERROR, "Failed to create thread");
        return req_generate_response_error(key, CON_SERVER_FAILURE);
    }
    if (selector_set_interest_key(key, OP_NOOP) != SELECTOR_SUCCESS) {
        return SOCKS_ERROR;
    }
    return REQ_RESOLVE;
}

unsigned request_read(selector_key * key) {
    our_log(INFO, "Request read started");
    client_data * data = ATTACHMENT(key);
    size_t bytes_available;
    uint8_t * ptr = buffer_write_ptr(&data->client_to_sv, &bytes_available);

    if (!buffer_can_write(&data->client_to_sv)) {
        return REQ_READ;
    }

    const ssize_t bytes_read = recv(key->fd, ptr, bytes_available, 0);

    if (bytes_read == 0) {
        our_log(INFO, "Client closed connection");
        return SOCKS_ERROR;
    }

    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return REQ_READ;
        }
        return SOCKS_ERROR;
    }

    buffer_write_adv(&data->client_to_sv, bytes_read);
    req_parse(&data->handshake.request_parser, &data->client_to_sv);

    if (is_req_done(&data->handshake.request_parser)) {
        if (req_has_error(&data->handshake.request_parser)) {
            struct sockaddr_storage dummy_bnd_addr = {0};
            dummy_bnd_addr.ss_family = AF_INET;

            if (req_generate_response(&data->handshake.request_parser, &data->sv_to_client, &dummy_bnd_addr, sizeof(dummy_bnd_addr)) || selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS) {
                return SOCKS_ERROR;
            }
            return REQ_WRITE;
        }
        return handle_request(key);
    }
    return REQ_READ;
}

unsigned request_write(selector_key * key) {
    our_log(INFO, "Request write started");
    client_data * data = ATTACHMENT(key);
    size_t bytes_to_write;
    uint8_t *ptr = buffer_read_ptr(&data->sv_to_client, &bytes_to_write);

    ssize_t bytes_sent = send(key->fd, ptr, bytes_to_write, MSG_NOSIGNAL);

    if (bytes_sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return REQ_WRITE;
        return SOCKS_ERROR;
    }

    if (bytes_sent == 0) {
        return SOCKS_ERROR;
    }

    buffer_read_adv(&data->sv_to_client, bytes_sent);
    if (buffer_can_read(&data->sv_to_client)) {
        return REQ_WRITE;
    }

    if (selector_set_interest_key(key, OP_READ) != SELECTOR_SUCCESS) {
        return SOCKS_ERROR;
    }

    char *uname = data->uname;
    StringBuilder *user_sb = get_access(uname);
    if(user_sb == NULL){
        user_sb = create_access(uname);
    }
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", tm_info);
    sb_append(user_sb, timestamp);
    sb_append(user_sb, "\t");
    sb_append(user_sb, uname);
    sb_append(user_sb, "\t");
    sb_append(user_sb, "A\t");

    if(data->client_address.ss_family == AF_INET){
        //IPV4
        struct sockaddr_in* info = (struct sockaddr_in*) (&data->client_address);
        char port_str[6]; 
        snprintf(port_str, sizeof(port_str), "%u", info->sin_port);
        char ip_str[INET_ADDRSTRLEN]; 
        inet_ntop(AF_INET, &info->sin_addr, ip_str, sizeof(ip_str));

        sb_append(user_sb, ip_str);
        sb_append(user_sb, "\t");
        sb_append(user_sb, port_str);
    }
    else if(data->client_address.ss_family == AF_INET6){
        //IPV6
        struct sockaddr_in6* info =(struct sockaddr_in6*) (&data->client_address);
        char port_str[6]; 
        snprintf(port_str, sizeof(port_str), "%u", info->sin6_port);
        char ip_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &info->sin6_addr, ip_str, sizeof(ip_str));

        sb_append(user_sb, ip_str);
        sb_append(user_sb, "\t");
        sb_append(user_sb, port_str);
    }
    
    sb_append(user_sb, "\t");
    sb_append(user_sb, (char *) data->handshake.request_parser.address.buffer);
    sb_append(user_sb, "\t");
    char port_str[6]; 
    snprintf(port_str, sizeof(port_str), "%u", data->handshake.request_parser.port);
    sb_append(user_sb, port_str);
    sb_append(user_sb, "\t");
    sb_append(user_sb, status_to_string(data->handshake.request_parser.connection_status));
    sb_append(user_sb, "\n");


    return COPY;
}

static void* request_resolve_domain_name(void* arg) {
    pthread_detach(pthread_self());

    resolve_job * rs = arg;
    client_data * data = rs->client_data;

    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;

    char * domain_name = data->handshake.request_parser.address.address_class.dn;
    char port[6] ={0};
    snprintf(port, 6,"%d", data->handshake.request_parser.port);

    int status = getaddrinfo(domain_name, port, &hints, &data->origin_addr);

    if (status != 0) {
        StringBuilder *sb = sb_create();
        sb_append(sb, "'getaddrinfo' error: ");
        sb_append(sb, gai_strerror(status));
        our_log(ERROR, sb_get_string(sb));
        sb_free(sb);
        data->origin_addr = NULL;
    }

    data->current_origin_addr = data->origin_addr;

    if (selector_notify_block(rs->selector, rs->client_fd)) {
        our_log(ERROR, "Failed to notify selecto of blocking job completion");
    }
    free(rs);
    return NULL;
}

unsigned request_resolve(selector_key * key) {
    our_log(INFO, "Request resolve (on_block_ready) started");
    client_data * data = ATTACHMENT(key);

    if (data->origin_addr == NULL) {
        return req_generate_response_error(key, CON_HOST_UNREACHABLE);
    }
    return initiate_origin_connection(key);
}

static unsigned initiate_origin_connection(selector_key * key) {
    client_data * data = ATTACHMENT(key);
    req_parser * rp = &data->handshake.request_parser;

    if (data->origin_fd != -1) {
        selector_unregister_fd(key->s, data->origin_fd);
        close(data->origin_fd);
        data->origin_fd = -1;
    }

    struct addrinfo * address = data->current_origin_addr;
    if (address == NULL) {
        data->origin_fd = -1;
        return req_generate_response_error(key, CON_HOST_UNREACHABLE);
    }

    data->origin_fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
    if (data->origin_fd == -1) {
        data->current_origin_addr = address->ai_next;
        return initiate_origin_connection(key);
    }

    if (selector_fd_set_nio(data->origin_fd) == -1) {
        close(data->origin_fd);
        data->origin_fd = -1;
        data->current_origin_addr = address->ai_next;
        return initiate_origin_connection(key);
    }

    const struct fd_handler *handlers = get_socks5_handlers();

    if (selector_register(key->s, data->origin_fd, handlers, OP_NOOP, data) != SELECTOR_SUCCESS) {
        close(data->origin_fd);
        data->origin_fd = -1;
        data->current_origin_addr = address->ai_next;
        return initiate_origin_connection(key);
    }

    int connection_result = connect(data->origin_fd, address->ai_addr, address->ai_addrlen);

    if (connection_result == 0) {
        rp->connection_status = CON_SUCCEEDED;

        // Get the bound address and port from the origin socket
        struct sockaddr_storage bnd_addr;
        socklen_t bnd_addr_len = sizeof(bnd_addr);
        if (getsockname(data->origin_fd, (struct sockaddr *)&bnd_addr, &bnd_addr_len) == -1) {
            perror("getsockname in initiate_origin_connection");
            return SOCKS_ERROR;
        }

        if (req_generate_response(rp, &data->sv_to_client, &bnd_addr, bnd_addr_len) || selector_set_interest_key(key, OP_WRITE) != SELECTOR_SUCCESS) {
            return SOCKS_ERROR;
        }

        if (selector_set_interest(key->s, data->origin_fd, OP_READ) != SELECTOR_SUCCESS) {
            return SOCKS_ERROR;
        }

        return REQ_CONNECT;
    }
    if (connection_result == -1 && errno == EINPROGRESS) {
        if (selector_set_interest(key->s, data->origin_fd, OP_WRITE) != SELECTOR_SUCCESS) {
            return SOCKS_ERROR;
        }
        if (selector_set_interest_key(key, OP_NOOP) != SELECTOR_SUCCESS) {
            return SOCKS_ERROR;
        }
        return REQ_CONNECT;
    }

    data->current_origin_addr = address->ai_next;
    return initiate_origin_connection(key);
}

unsigned request_connect(selector_key * key) {
    our_log(INFO,"Request conncect (on_write_ready) started");
    client_data * data = ATTACHMENT(key);
    req_parser * rp = &data->handshake.request_parser;

    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(data->origin_fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
        return req_generate_response_error(key, CON_SERVER_FAILURE);
    }

    if (error == 0) {
        rp->connection_status = CON_SUCCEEDED;


        if (data->origin_addr != NULL) {
            if (data->handshake.request_parser.atyp == DN) {
                freeaddrinfo(data->origin_addr);
            } else {
                if (data->origin_addr->ai_addr != NULL) {
                    free(data->origin_addr->ai_addr);
                }
                free(data->origin_addr);
            }
            data->origin_addr = NULL;
            data->current_origin_addr = NULL;
        }

        struct sockaddr_storage bnd_addr;
        socklen_t bnd_addr_len = sizeof(bnd_addr);
        if (getsockname(data->origin_fd, (struct sockaddr *)&bnd_addr, &bnd_addr_len) == -1) {
            return SOCKS_ERROR;
        }

        if (req_generate_response(rp, &data->sv_to_client, &bnd_addr, bnd_addr_len) || selector_set_interest(key->s, data->client_fd, OP_WRITE) != SELECTOR_SUCCESS) {
            return SOCKS_ERROR;
        }

        if (selector_set_interest(key->s, data->origin_fd, OP_READ) != SELECTOR_SUCCESS) {
            return SOCKS_ERROR;
        }
        return REQ_WRITE;
    }

    struct addrinfo * address = data->current_origin_addr->ai_next;
    selector_unregister_fd(key->s, data->origin_fd);
    close(data->origin_fd);
    data->origin_fd = -1;
    if (address == NULL) {
        return req_generate_response_error(key, CON_HOST_UNREACHABLE);
    }
    return initiate_origin_connection(key);
}

