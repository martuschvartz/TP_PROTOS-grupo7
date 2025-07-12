#include <manager.h>

/*---------------------------------------------------

FALTA:
    * comandos STAT, LIST-USER,

TODO @josefina
    * autenticación
    * conectar bien los datos reales
    * modularizar y ordenar todo mejor

    * que cuando se liste los usuarios, se muestre el status (y si puede ser más lindo, mejor)

---------------------------------------------------*/

#define PORT 1081 // debería ser variable!!
#define BUFFER_SIZE 1024
#define MAX_USERS 10

#define OK "+OK "
#define ERR "-ERR "
#define OK_MSG(s) OK s
#define ERR_MSG(s) ERR s

typedef struct
{
    int fd;
    char buffer[BUFFER_SIZE];

    bool logged_in;
    bool user_sent;
    char username[50];
    bool is_admin;
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

void send_response(int client_fd, const char *msg, bool is_error)
{
    char full_msg[BUFFER_SIZE];

    snprintf(full_msg, sizeof(full_msg), "%s%s", is_error ? ERR : OK, msg);
    send(client_fd, full_msg, strlen(full_msg), 0);
}

void handle_auth_command(int client_fd, char *cmd, char *arg1, manager_data *data)
{
    if (strcmp(cmd, "USER") == 0)
    {
        if (arg1)
        {
            int index = user_exists(arg1);
            if (index >= 0)
            {
                strncpy(data->username, arg1, sizeof(data->username) - 1);
                data->user_sent = true;
                send_response(client_fd, "\r\n", false);
            }
            else
            {
                send_response(client_fd, "usuario no reconocido\r\n", true);
            }
        }
        else
        {
            send_response(client_fd, "falta nombre de usuario\r\n", true);
        }
    }
    else if (strcmp(cmd, "PASS") == 0)
    {
        if (!data->user_sent)
        {
            send_response(client_fd, "primero manda USER\r\n", true);
        }
        else if (arg1)
        {
            if (user_login(data->username, arg1) == 0)
            {
                int index = user_exists(data->username);
                if (index >= 0)
                {
                    const Tuser *ulist = get_users();
                    if (ulist[index].status == ADMIN)
                    {
                        data->logged_in = true;
                        data->is_admin = true;
                        send_response(client_fd, "autenticado como admin\r\n", false);
                    }
                    else
                    {
                        send_response(client_fd, "necesitas ser administrador\r\n", true);
                    }
                }
            }
            else
                send_response(client_fd, "contraseña incorrecta\r\n", true);
        }
    }

    else
    {
        send_response(client_fd, "debes autenticarte primero (USER + PASS)\r\n", true);
    }
}

// FALTA MODULARIZAR (@josefina)
void handle_command(int client_fd, char *input, manager_data *manager_data)
{
    char *cmd = strtok(input, " ");
    char *arg1 = strtok(NULL, " ");
    char *arg2 = strtok(NULL, " ");

    if (!cmd)
        return;

    if (!manager_data->logged_in)
    {
        handle_auth_command(client_fd, cmd, arg1, manager_data);
        return;
    }

    if (strcmp(cmd, "LIST") == 0)
    {
        const Tuser *ulist = get_users();
        unsigned int count = get_user_count();

        char msg[1024] = "Usuarios:\r\n";
        for (unsigned int i = 0; i < count; i++)
        {
            strcat(msg, " - ");
            strcat(msg, ulist[i].name);
            strcat(msg, "\r\n");
        }
        send_response(client_fd, msg, false);
    }
    else if (strcmp(cmd, "ADD-USER") == 0)
    {
        if (arg1 && arg2)
        {
            if (new_user(arg1, arg2) == 0)
            {
                char msg[128];
                snprintf(msg, sizeof(msg), "Usuario %s agregado\r\n", arg1);
                send_response(client_fd, msg, false);
            }
            else
            {
                send_response(client_fd, "No se pudo agregar el usuario\r\n", true);
            }
        }
        else
        {
            send_response(client_fd, "Faltan parámetros para ADD-USER\r\n", true);
        }
    }
    else if (strcmp(cmd, "DELETE-USER") == 0)
    {
        if (arg1)
        {
            if (delete_user(arg1) == 0)
            {
                char msg[128];
                snprintf(msg, sizeof(msg), "Usuario %s eliminado\r\n", arg1);
                send_response(client_fd, msg, false);
            }
            else
            {
                send_response(client_fd, "No se pudo eliminar el usuario\r\n", true);
            }
        }
        else
        {
            send_response(client_fd, "Falta parámetro para DELETE-USER\r\n", true);
        }
    }
    else if (strcmp(cmd, "CHANGE-PASS") == 0)
    {
        if (arg1 && arg2)
        {
            if (change_password(manager_data->username, arg1, arg2) == 0)
            {
                send_response(client_fd, "Contraseña actualizada\r\n", false);
            }
            else
            {
                send_response(client_fd, "No se pudo cambiar la contraseña\r\n", true);
            }
        }
        else
        {
            send_response(client_fd, "Faltan parámetros: CHANGE-PASS <vieja> <nueva>\r\n", true);
        }
    }
    else if (strcmp(cmd, "LIST-USER") == 0)
    {
        send_response(client_fd, "TO-DO\r\n", true);
    }
    else if (strcmp(cmd, "STAT") == 0)
    {
        send_response(client_fd, "TO-DO\r\n", true);
    }
    else if (strcmp(cmd, "CHANGE-STATUS") == 0)
    {
        if (arg1 && arg2)
        {
            change_status(arg1, strcmp(arg2, "admin") == 0 ? ADMIN : COMMONER);
            char msg[128];
            snprintf(msg, sizeof(msg), "Estado de %s cambiado a %s\r\n", arg1, strcmp(arg2, "admin") == 0 ? "admin" : "commoner");
            send_response(client_fd, msg, false);
        }
        else
        {
            send_response(client_fd, "Faltan parámetros: CHANGE-STATUS <usuario> <admin|commoner>", true);
        }
    }
    else if (strcmp(cmd, "HELP") == 0)
    {
        send_response(client_fd,
                      "Comandos válidos:\r\n"
                      " - LIST\r\n"
                      " - ADD-USER <usuario> <clave>\r\n"
                      " - DELETE-USER <usuario>\r\n"
                      " - CHANGE-PASS <vieja> <nueva>\r\n"
                      " - STAT\r\n"
                      " - CHANGE-STATUS <usuario> <admin|commoner>\r\n"
                      " - HELP\r\n"
                      " - QUIT\r\n",
                      false);
    }
    else if (strcmp(cmd, "QUIT") == 0)
    {
        send_response(client_fd, "Se cerrará la conexión\r\n", false);
        close(client_fd);
    }
    else
    {
        send_response(client_fd, "Comando no reconocido. Escriba HELP\r\n", true);
    }
}
void manager_passive_accept(struct selector_key *key)
{
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client = accept(key->fd, (struct sockaddr *)&client_addr, &client_addr_len);
    send_response(client, "Dory v1.0\r\n", false);
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
    handle_command(data->fd, data->buffer, data);
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