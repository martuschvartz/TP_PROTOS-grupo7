#include <request.h>
#include <request_parser.h>
#include <socks5.h>

void request_read_init(const unsigned state, selector_key * key) {
    client_data * data = ATTACHMENT(key);
    initialize_request_parser(&data->handshake.request_parser);
}
