#include <request_parser.h>
#include <string.h>

#define PROTOCOL_VERSION 0x05
#define CONNECT 0x01
#define RSV 0x00

void initialize_request_parser(req_parser *rp) {
    if (rp != NULL) {
        rp->current_state = REQ_VERSION;
        rp->connection_status = CON_SERVER_FAILURE;
        rp->addr_len = 0;
        rp->bytes_read = 0;
        rp->port = 0;
    }
}

void req_parse_version(req_parser *rp, uint8_t version) {
    if (version == PROTOCOL_VERSION) {
        rp->current_state = REQ_CMD;
        return;
    }
    rp->current_state = REQ_ERROR;
}

void req_parse_cmd(req_parser *rp, uint8_t cmd) {
    if (cmd == CONNECT) {
        rp->current_state = REQ_RSV;
        return;
    }
    rp->current_state = REQ_ERROR;
    rp->connection_status = CON_COMMAND_NOT_SUPPORTED;
}

void req_parse_rsv(req_parser *rp, uint8_t rsv) {
    if (rsv == RSV) {
        rp->current_state = REQ_ATYP;
        return;
    }
    rp->current_state = REQ_ERROR;
    rp->connection_status = CON_SERVER_FAILURE;
}

void req_parse_atyp(req_parser *rp, uint8_t atyp) {
    rp->atyp = atyp;
    if (atyp == IPV4) {
        rp->current_state = REQ_DST_ADDR;
        rp->addr_len = sizeof(rp->address.address_class.ipv4);
    }
    else if (atyp == IPV6) {
        rp->current_state = REQ_DST_ADDR;
        rp->addr_len = sizeof(rp->address.address_class.ipv6);
    }
    else if (atyp == DN) {
        rp->current_state = REQ_DST_ADDR_LENGTH;
    }
    else {
        rp->current_state = REQ_ERROR;
        rp->connection_status = CON_ATYPE_NOT_SUPPORTED;
    }
}

void req_parse_dst_addr_length(req_parser *rp, uint8_t dn_length) {
    if (dn_length > MAX_ADDR_LENGTH) {
        rp->current_state = REQ_ERROR;
        rp->connection_status = CON_OTHER;
        return;
    }
    rp->addr_len = dn_length;
    rp->current_state = REQ_DST_ADDR;
}

void req_parse_dst_addr(req_parser *rp, uint8_t byte) {
    rp->address.buffer[rp->bytes_read++] = byte;
    if (rp->bytes_read == rp->addr_len) {
        switch (rp->atyp) {
            case IPV4:
                memcpy(&rp->address.address_class.ipv4, rp->address.buffer, rp->addr_len);
                break;
            case IPV6:
                memcpy(&rp->address.address_class.ipv6, rp->address.buffer, rp->addr_len);
                break;
            case DN:
                memcpy(rp->address.address_class.dn, rp->address.buffer, rp->addr_len);
                rp->address.address_class.dn[rp->addr_len] = '\0';
                break;
            default:
                return;
        }
        rp->current_state = REQ_DST_PORT;
        rp->bytes_read = 0;
        return;
    }
    rp->current_state = REQ_DST_ADDR;
}

void req_parse_dst_port(req_parser *rp, uint8_t byte) {
    if (rp->bytes_read == 0) {
        rp->bytes_read++;
        rp->port = (in_port_t)(byte << 8);
    }
    else {
        rp->port |= byte;
        rp->bytes_read = 0;
        rp->current_state = REQ_DONE;
    }
}

int req_has_error(req_parser *rp) {
    return rp->current_state == REQ_ERROR;
}

int is_req_done(req_parser *rp) {
    return rp->current_state == REQ_DONE || rp->current_state == REQ_ERROR;
}

int req_generate_response(req_parser *rp, buffer *buffer, const struct sockaddr_storage *bnd_addr) {
    uint8_t response[1 + 1 + 1 + 1 + 16 + 2];
    int offset = 0;

    response[offset++] = PROTOCOL_VERSION;
    response[offset++] = rp->connection_status;
    response[offset++] = RSV;

    if (rp->connection_status == CON_SUCCEEDED) {
        if (bnd_addr->ss_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)bnd_addr;
            response[offset++] = IPV4;
            memcpy(&response[offset], &ipv4->sin_addr.s_addr, 4);
            offset += 4;
            memcpy(&response[offset], &ipv4->sin_port, 2);
            offset += 2;
        } else if (bnd_addr->ss_family == AF_INET6) {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)bnd_addr;
            response[offset++] = IPV6;
            memcpy(&response[offset], &ipv6->sin6_addr.s6_addr, 16);
            offset += 16;
            memcpy(&response[offset], &ipv6->sin6_port, 2);
            offset += 2;
        } else {
            response[offset++] = IPV4;
            memset(&response[offset], 0, 4);
            offset += 4;
            memset(&response[offset], 0, 2);
            offset += 2;
        }
    } else {
        response[offset++] = IPV4;
        memset(&response[offset], 0, 4);
        offset += 4;
        memset(&response[offset], 0, 2);
        offset += 2;
    }


    for (int i = 0; i < offset; i++) {
        if (!buffer_can_write(buffer)) {
            return 1;
        }
        buffer_write(buffer, response[i]);
    }
    return 0;
}

void req_parse(req_parser *rp, buffer *buffer) {
    while (buffer_can_read(buffer) && rp->current_state != REQ_DONE && rp->current_state != REQ_ERROR) {
        switch (rp->current_state) {
            case REQ_VERSION:
                req_parse_version(rp, buffer_read(buffer));
                break;
            case REQ_CMD:
                req_parse_cmd(rp, buffer_read(buffer));
                break;
            case REQ_RSV:
                req_parse_rsv(rp, buffer_read(buffer));
                break;
            case REQ_ATYP:
                req_parse_atyp(rp, buffer_read(buffer));
                break;
            case REQ_DST_ADDR_LENGTH:
                req_parse_dst_addr_length(rp, buffer_read(buffer));
                break;
            case REQ_DST_ADDR:
                req_parse_dst_addr(rp, buffer_read(buffer));
                break;
            case REQ_DST_PORT:
                req_parse_dst_port(rp, buffer_read(buffer));
                break;
            default:
                break;
        }
    }
}
