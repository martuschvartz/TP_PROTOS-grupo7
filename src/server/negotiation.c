#include <negotiation.h>

void negotiation_read_init(const unsigned state, selector_key *key) {
    //inicializa el parser
    client_data * client_data = ATTACHMENT(key);
    initialize_negotiation_parser(&client_data->client.negotiation.parser);
}

void negotiation_write(selector_key *key) {
}

void negotiation_read(selector_key *key) {
}
