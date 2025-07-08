#include <manager.h>

/*---------------------------------------------------

FALTA:
    * comandos STAT, LIST-USER, CHANGE-PASS

TODO @josefina
    * autenticación
    * conectar bien los datos reales
    * modularizar y ordenar todo mejor

---------------------------------------------------*/

#define PORT 1081 // debería ser variable!!
#define BUFFER_SIZE 1024
#define MAX_USERS 10

#define OK "+OK"
#define ERR "-ERR"
#define OK_MSG(s) OK m "\r\n"
#define ERR_MSG(s) ERR m "\r\n"

typedef struct
{
    int fd;
    char buffer[BUFFER_SIZE];
} manager_data;

static void manager_read(struct selector_key *key);
static void manager_close(struct selector_key *key);

static const struct fd_handler manager_handler = {
    .handle_read = manager_read,
    .handle_write = NULL,
    .handle_close = manager_close,
};

void trim_newline(char *str)
{
    str[strcspn(str, "\r\n")] = 0;
}

void send_response(int client_fd, const char *msg)
{
    send(client_fd, msg, strlen(msg), 0);
}

// FALTA MODULARIZAR, y usar las macros OK_MSG y ERR_MSG (@josefina)
void handle_command(int client_fd, char *input)
{
    char *cmd = strtok(input, " ");
    char *arg1 = strtok(NULL, " ");
    char *arg2 = strtok(NULL, " ");

    if (!cmd)
        return;

    if (strcmp(cmd, "LIST") == 0)
    {
        const user *ulist = getUsers();
        unsigned int count = getUserCount();

        char msg[1024] = "+OK Usuarios:\r\n";
        for (unsigned int i = 0; i < count; i++)
        {
            strcat(msg, " - ");
            strcat(msg, ulist[i].name);
            strcat(msg, "\r\n");
        }
        send_response(client_fd, msg);
    }
    else if (strcmp(cmd, "ADD-USER") == 0)
    {
        if (arg1 && arg2)
        {
            if (newUser(arg1, arg2) == 0)
            {
                char msg[128];
                snprintf(msg, sizeof(msg), "+OK Usuario %s agregado\r\n", arg1);
                send_response(client_fd, msg);
            }
            else
            {
                send_response(client_fd, "-ERR No se pudo agregar el usuario\r\n");
            }
        }
        else
        {
            send_response(client_fd, "-ERR Faltan parámetros para ADD-USER\r\n");
        }
    }
    else if (strcmp(cmd, "DELETE-USER") == 0)
    {
        if (arg1)
        {
            if (deleteUser(arg1) == 0)
            {
                char msg[128];
                snprintf(msg, sizeof(msg), "+OK Usuario %s eliminado\r\n", arg1);
                send_response(client_fd, msg);
            }
            else
            {
                send_response(client_fd, "-ERR No se pudo eliminar el usuario\r\n");
            }
        }
        else
        {
            send_response(client_fd, "-ERR Falta parámetro para DELETE-USER\r\n");
        }
    }
    else if (strcmp(cmd, "CHANGE-PASS") == 0)
    {
        send_response(client_fd, "TO-DO\r\n");
    }
    else if (strcmp(cmd, "LIST-USER") == 0)
    {
        send_response(client_fd, "TO-DO\r\n");
    }
    else if (strcmp(cmd, "STAT") == 0)
    {
        send_response(client_fd, "TO-DO\r\n");
    }
    else if (strcmp(cmd, "HELP") == 0)
    {
        send_response(client_fd,
                      "+OK Comandos válidos:\r\n"
                      " - LIST\r\n"
                      " - ADD-USER <usuario> <clave>\r\n"
                      " - DELETE-USER <usuario>\r\n"
                      " - CHANGE-PASS <vieja> <nueva>\r\n"
                      " - STAT\r\n"
                      " - HELP\r\n"
                      " - QUIT\r\n");
    }
    else if (strcmp(cmd, "QUIT") == 0)
    {
        send_response(client_fd, "+OK chau\r\n");
        close(client_fd);
    }
    else
    {
        send_response(client_fd, "-ERR Comando no reconocido. Escriba HELP\r\n");
    }
}
void manager_passive_accept(struct selector_key *key)
{
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client = accept(key->fd, (struct sockaddr *)&client_addr, &client_addr_len);
    send_response(client, "+OK Dory monitoreo :) \r\n");
    if (client < 0)
        return;

    if (selector_fd_set_nio(client) == -1)
    {
        close(client);
        return;
    }

    manager_data *data = calloc(1, sizeof(manager_data));
    if (data == NULL)
    {
        close(client);
        return;
    }

    data->fd = client;

    if (SELECTOR_SUCCESS != selector_register(key->s, client, &manager_handler, OP_READ, data))
    {
        close(client);
        free(data);
    }
}

static void manager_read(struct selector_key *key)
{
    manager_data *data = key->data;
    int n = recv(data->fd, data->buffer, BUFFER_SIZE - 1, 0);
    if (n <= 0)
    {
        selector_unregister_fd(key->s, data->fd);
        close(data->fd);
        free(data);
        return;
    }
    data->buffer[n] = 0;
    data->buffer[strcspn(data->buffer, "\r\n")] = 0;
    handle_command(data->fd, data->buffer);
}

static void manager_close(struct selector_key *key)
{
    manager_data *data = key->data;
    if (data != NULL)
    {
        close(data->fd);
        free(data);
    }
}