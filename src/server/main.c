#include <stdio.h>
#include <signal.h>
#include <selector.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <unistd.h>

static bool done = false;

struct data_info{
    void * data;
    int length;
};

static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}

void
echo_passive_read(struct selector_key *key){
    struct data_info * dataInfo = key->data;
    selector_set_interest(key->s, key->fd, OP_READ | OP_WRITE);
    char buffer[1024];
    int size = recv(key->fd, buffer, 1024, 0);
    dataInfo->length = size;
    memcpy(dataInfo->data,buffer, size);
}

void
echo_passive_write(struct selector_key *key){
    struct data_info * dataInfo = key->data;
    send(key->fd, dataInfo->data, dataInfo->length, 0);
    selector_set_interest(key->s, key->fd, OP_READ);
}

void
echo_passive_close(struct selector_key *key){
    puts("bye!");
}

void
echo_passive_accept(struct selector_key *key) {
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len = sizeof(client_addr);

    const int client = accept(key->fd, (struct sockaddr*) &client_addr,
                                                          &client_addr_len);
    if(client == -1) {
        return;
    }
    if(selector_fd_set_nio(client) == -1) {
        return;
    }

    const struct fd_handler socksv5 = {
        .handle_read       = echo_passive_read,
        .handle_write      = echo_passive_write,
        .handle_close      = echo_passive_close,
    };

    struct data_info * dataInfo = malloc(sizeof(struct data_info));
    dataInfo->data = malloc(1024);
    dataInfo->length = 0;

    if(SELECTOR_SUCCESS != selector_register(key->s, client, &socksv5,
                                              OP_READ, dataInfo)) {
        return;
    }
    return ;
}


int main(void){
    int port = 8080;
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);
    

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(server < 0) {
        fprintf(stderr, "unable to create server socket :(");
        return 1;
    }

    if(selector_fd_set_nio(server)) {
        fprintf(stderr, "unable to set non block on server socket :(");
        return 1;
    }

    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

    if(bind(server, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        fprintf(stderr, "unable to bind server socket :(");
        return 1;
    }

    if (listen(server, 20) < 0) {
        fprintf(stderr, "unable to listen server socket :(");
        return 1;
    }

    fprintf(stdout, "Listening on TCP port %d\n", port);
    
    const struct selector_init conf = {
        .signal = SIGALRM,
        .select_timeout = {
            .tv_sec  = 10,
            .tv_nsec = 0,
        },
    };

    if(selector_init(&conf)){
        fprintf(stderr, "error initializing selector :(");
        return 1;
    }

    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;

    selector = selector_new(1024);
    if(selector == NULL) {
        fprintf(stderr, "unable to create selector :(");
        return 1;
    }
    const struct fd_handler socksv5 = {
        .handle_read       = echo_passive_accept,
        .handle_write      = NULL,
        .handle_close      = NULL,
    };
    ss = selector_register(selector, server, &socksv5,
                                              OP_READ, NULL);
    if(ss != SELECTOR_SUCCESS) {
        fprintf(stderr, "error registering fd :(");
        return 1;
    }
    for(;!done;) {
        ss = selector_select(selector);
        if(ss != SELECTOR_SUCCESS) {
            fprintf(stderr, "error serving :(");
            return 1;
        }
    }

    selector_destroy(selector);
    selector_close();

    if(server >= 0) {
        close(server);
    }

    return 0;
}
