#include <manager.h>

#define BUFFER_SIZE 1024
#define MAX_USERS 10

#define OK "+OK "
#define ERR "-ERR "
#define OK_MSG(s) OK s
#define ERR_MSG(s) ERR s

#define MAX_LENGTH_MSG 512

typedef struct
{
    int fd;
    char buffer[BUFFER_SIZE];

    bool logged_in;
    bool user_sent;
    char username[50];
    bool is_admin;

    fd_selector selector;
} manager_data;

static void manager_read(struct selector_key *key);
static void manager_close(struct selector_key *key);

// manejo de comandos
void handle_list(int fd);
void handle_add_user(int fd, char *user, char *pass);
void handle_delete_user(int fd, char *user);
void handle_change_pass(int fd, manager_data *data, char *old_pass, char *new_pass);
void handle_change_status(int fd, char *user, char *status);
void handle_stat(int fd);
void handle_list_user(int fd, char *user);
void handle_quit(manager_data *data);
void handle_help(int fd);
void handle_server_logs(int fd);
void handle_set_max_users(int fd, char *arg);

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
    const char *prefix = is_error ? ERR : OK;
    size_t prefix_len = strlen(prefix);
    size_t msg_len = strlen(msg);

    size_t total_len = prefix_len + msg_len;
    char *full_msg = malloc(total_len + 1); // +1 para el '\0'
    if (!full_msg)
    {
        send(client_fd, "-ERR Error interno de memoria\r\n", 31, 0);
        return;
    }

    memcpy(full_msg, prefix, prefix_len);
    memcpy(full_msg + prefix_len, msg, msg_len);
    full_msg[total_len] = '\0';

    send(client_fd, full_msg, total_len, 0);
    free(full_msg);
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
                send_response(client_fd, "Nombre de usuario no encontrado\r\n", true);
            }
        }
        else
        {
            send_response(client_fd, "Falta parámetro: USER <nombre_usuario>\r\n", true);
        }
    }
    else if (strcmp(cmd, "PASS") == 0)
    {
        if (!data->user_sent)
        {
            send_response(client_fd, "Error de protocolo: se esperaba el comando USER antes de PASS\r\n", true);
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
                        send_response(client_fd, "Autenticación exitosa. Privilegios de administrador concedidos.\r\n", false);
                    }
                    else
                    {
                        send_response(client_fd, "Acceso denegado. El usuario no posee privilegios administrativos\r\n", true);
                    }
                }
            }
            else
                send_response(client_fd, "Error de autenticación: contraseña inválida\r\n", true);
        }
    }

    else
    {
        send_response(client_fd, "Autenticación requerida: Utilizar USER y PASS\r\n", true);
    }
}

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

    if (strcmp(cmd, "LIST-USERS") == 0)
    {
        handle_list(client_fd);
    }
    else if (strcmp(cmd, "ADD-USER") == 0)
    {
        handle_add_user(client_fd, arg1, arg2);
    }
    else if (strcmp(cmd, "DELETE-USER") == 0)
    {
        handle_delete_user(client_fd, arg1);
    }
    else if (strcmp(cmd, "CHANGE-PASS") == 0)
    {
        handle_change_pass(client_fd, manager_data, arg1, arg2);
    }
    else if (strcmp(cmd, "USER-LOGS") == 0)
    {
        handle_list_user(client_fd, arg1);
    }
    else if (strcmp(cmd, "STATS") == 0)
    {
        handle_stat(client_fd);
    }
    else if (strcmp(cmd, "CHANGE-STATUS") == 0)
    {
        handle_change_status(client_fd, arg1, arg2);
    }
    else if (strcmp(cmd, "HELP") == 0)
    {
        handle_help(client_fd);
    }
    else if (strcmp(cmd, "QUIT") == 0)
    {
        handle_quit(manager_data);
    }
    else if (strcmp(cmd, "SERVER-LOGS") == 0)
    {
        handle_server_logs(client_fd);
    }
    else if (strcmp(cmd, "SET-MAX-USERS") == 0)
    {
        handle_set_max_users(client_fd, arg1);
    }
    else if (strcmp(cmd, "GET-MAX-USERS") == 0)
    {
        char msg[MAX_LENGTH_MSG];
        snprintf(msg, sizeof(msg), "La capacidad máxima de usuarios es: %u.\r\n", max_users);
        send_response(client_fd, msg, false);
    }
    else
    {
        send_response(client_fd, "Comando desconocido. Utilice HELP para ver las opciones disponibles.\r\n", true);
    }
}

// conexiones --------------------------------------------------------------
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
    data->selector = key->s;

    if (SELECTOR_SUCCESS != selector_register(key->s, client, &manager_handler, OP_READ, data))
    {
        close(client);
        free(data);
    }
}

static void manager_read(struct selector_key *key)
{
    manager_data *data = key->data;
    if (data == NULL)
        return;
    int n = recv(data->fd, data->buffer, BUFFER_SIZE - 1, 0);
    if (n <= 0)
    {
        selector_unregister_fd(key->s, data->fd);
        return;
    }
    data->buffer[n] = 0;
    data->buffer[strcspn(data->buffer, "\r\n")] = 0;
    if (strlen(data->buffer) == 0)
    {
        // línea vacía
        send_response(data->fd, "No se recibió ningún comando. Escriba HELP para ver opciones.\r\n", true);
        return;
    }
    handle_command(data->fd, data->buffer, data);
}

static void manager_close(struct selector_key *key)
{
    manager_data *data = key->data;
    if (data != NULL)
    {
        close(data->fd);
        free(data);
        key->data = NULL;
    }
}

// ---------------------------------------------------------------

void handle_list(int fd)
{
    const Tuser *ulist = get_users();
    unsigned int count = get_user_count();

    StringBuilder *sb = sb_create();
    if (!sb)
    {
        send_response(fd, "Error interno de memoria\r\n", true);
        return;
    }

    sb_append(sb, "Usuarios:\r\n");

    for (unsigned int i = 0; i < count; i++)
    {
        sb_append(sb, " - ");
        sb_append(sb, ulist[i].name);
        sb_append(sb, " (");
        sb_append(sb, ulist[i].status == ADMIN ? "admin" : "commoner");
        sb_append(sb, ")\r\n");
    }

    send_response(fd, sb_get_string(sb), false);
    sb_free(sb);
}

void handle_add_user(int fd, char *user, char *pass)
{
    if (!user || !pass)
    {
        send_response(fd, "Faltan parámetros: ADD-USER <usuario> <constraseña>\r\n", true);
        return;
    }
    if (new_user(user, pass) == 0)
    {
        char msg[MAX_LENGTH_MSG];
        snprintf(msg, sizeof(msg), "Usuario %s creado exitosamente\r\n", user);
        send_response(fd, msg, false);
    }
    else
    {
        send_response(fd, "Error al crear el usuario. Verifique que no exista previamente.\r\n", true);
    }
}

void handle_delete_user(int fd, char *user)
{
    if (!user)
    {
        send_response(fd, "Falta parámetro para DELETE-USER\r\n", true);
        return;
    }
    if (delete_user(user) == 0)
    {
        char msg[MAX_LENGTH_MSG];
        snprintf(msg, sizeof(msg), "Usuario %s eliminado correctamente\r\n", user);
        send_response(fd, msg, false);
    }
    else
    {
        send_response(fd, "Error al eliminar el usuario. Verifique que el usuario exista.\r\n", true);
    }
}

void handle_change_pass(int fd, manager_data *data, char *old_pass, char *new_pass)
{
    if (!old_pass || !new_pass)
    {
        send_response(fd, "Faltan parámetros: CHANGE-PASS <vieja> <nueva>\r\n", true);
        return;
    }

    if (change_password(data->username, old_pass, new_pass) == 0)
    {
        send_response(fd, "La contraseña fue modificada con éxito.\r\n", false);
    }
    else
    {
        send_response(fd, "No se pudo cambiar la contraseña\r\n", true);
    }
}

void handle_change_status(int fd, char *user, char *status)
{
    if (!user || !status)
    {

        send_response(fd, "Faltan parámetros: CHANGE-STATUS <usuario> <rol>", true);
        return;
    }
    change_status(user, strcmp(status, "admin") == 0 ? ADMIN : COMMONER);
    char msg[MAX_LENGTH_MSG];
    snprintf(msg, sizeof(msg), "El rol del usuario %s fue actualizado a %s.\r\n", user, strcmp(status, "admin") == 0 ? "admin" : "commoner");
    send_response(fd, msg, false);
}

void handle_stat(int fd)
{
    int current_connections = get_connections();
    int historical_connections = get_historical_connections();
    unsigned int bytes = get_bytes();
    unsigned int count = get_user_count();
    char msg[MAX_LENGTH_MSG];
    snprintf(msg, sizeof(msg), "\r\n-Total usuarios concurrentes: %u\r\n-Conexiones concurrentes: %d\r\n-Conexiones históricas: %d\r\n-Bytes transferidos: %d\r\n", count, current_connections, historical_connections, bytes);
    send_response(fd, msg, false);
}

void handle_quit(manager_data *data)
{
    send_response(data->fd, "Sesión finalizada. Cerrando la conexión.\r\n", false);
    selector_unregister_fd(data->selector, data->fd);
}

void handle_list_user(int fd, char *user)
{
    if (!user)
    {
        send_response(fd, "Falta parámetro: LIST-USER <usuario>\r\n", true);
        return;
    }

    int index = user_exists(user);
    if (index < 0)
    {
        char err_msg[MAX_LENGTH_MSG];
        snprintf(err_msg, sizeof(err_msg), "El usuario '%s' no existe\r\n", user);
        send_response(fd, err_msg, true);
        return;
    }

    const Tuser *u = &get_users()[index];
    char msg[2048];
    memset(msg, 0, sizeof(msg));

    if (u->access != NULL)
    {
        const char *access_list = sb_get_string(u->access);
        if (access_list != NULL && strlen(access_list) > 0)
        {
            strcat(msg, access_list);
            strcat(msg, "\r\n");
        }
        else
        {
            strcat(msg, "(sin accesos registrados)\r\n");
        }
    }
    else
    {
        strcat(msg, "(sin accesos registrados)\r\n");
    }

    strcat(msg, "----------------------------------\r\n");

    send_response(fd, msg, false);
}

void handle_server_logs(int fd)
{
    const char *logs = get_logs();
    if (logs && strlen(logs) > 0)
    {
        StringBuilder *sb = sb_create();
        if (!sb)
        {
            send_response(fd, "Error interno de memoria\r\n", true);
            return;
        }

        sb_append(sb, "\n");
        sb_append(sb, logs);
        sb_append(sb, "----------------------------------\r\n");

        send_response(fd, sb_get_string(sb), false);
        sb_free(sb);
    }
    else
    {
        send_response(fd, "No hay logs registrados.\r\n", false);
    }
}

void handle_set_max_users(int fd, char *arg)
{
    if (!arg)
    {
        send_response(fd, "Falta parámetro: SET-MAX-USERS <numero>\r\n", true);
        return;
    }

    char *endptr;
    long new_limit = strtol(arg, &endptr, 10);

    if (*endptr != '\0' || new_limit <= 0)
    {
        send_response(fd, "Valor inválido. Debe ser un número positivo.\r\n", true);
        return;
    }

    if ((unsigned int)new_limit < cantUsers)
    {
        send_response(fd, "No se puede reducir por debajo del número actual de usuarios.\r\n", true);
        return;
    }

    // Redimensionamos el array
    Tuser *new_users = realloc(users, new_limit * sizeof(Tuser));
    if (new_users == NULL)
    {
        send_response(fd, "Error de memoria al redimensionar.\r\n", true);
        return;
    }

    users = new_users;
    max_users = (unsigned int)new_limit;

    char msg[MAX_LENGTH_MSG];
    snprintf(msg, sizeof(msg), "Capacidad máxima de usuarios actualizada a %u.\r\n", max_users);
    send_response(fd, msg, false);

    our_log(INFO, "User array resized successfully");
}

void handle_help(int fd)
{
    send_response(fd,
                  "Comandos disponibles:\r\n"
                  " - LIST-USERS                        Listar todos los usuarios registrados\r\n"
                  " - ADD-USER <usuario> <contraseña>   Crear un nuevo usuario\r\n"
                  " - DELETE-USER <usuario>             Eliminar un usuario existente\r\n"
                  " - CHANGE-PASS <vieja> <nueva>       Modificar su propia contraseña\r\n"
                  " - CHANGE-STATUS <usuario> <rol>     Cambiar el rol (\"admin\" o \"commoner\") de un usuario\r\n"
                  " - USER-LOGS <usuario>               Mostrar accesos registrados del usuario\r\n"
                  " - SERVER-LOGS                       Ver los logs del servidor\r\n"
                  " - SET-MAX-USERS <numero>            Definir la capacidad máxima de usuarios\r\n"
                  " - GET-MAX-USERS                     Consultar la capacidad máxima de usuarios\r\n"
                  " - STATS                             Ver métricas de uso del sistema\r\n"
                  " - HELP                              Mostrar esta ayuda\r\n"
                  " - QUIT                              Finalizar la sesión actual\r\n",
                  false);
}
