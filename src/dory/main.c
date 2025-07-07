#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>    // close()
#include <arpa/inet.h> // sockaddr_in, inet_addr
#include <ctype.h>

#define PORT 1081 // debería ser variable!!
#define BUFFER_SIZE 1024

#define OK "+OK"
#define ERR "-ERR"
#define OK_MSG(s) OK m "\r\n"
#define ERR_MSG(s) ERR m "\r\n"

void trim_newline(char *str)
{
    str[strcspn(str, "\r\n")] = 0;
}

void send_response(int client_fd, const char *msg)
{
    send(client_fd, msg, strlen(msg), 0);
}

void handle_list(int client_fd)
{
    send_response(client_fd, "Usuarios falsos:\r\n - jose\r\n - martu\r\n - thomas\r\n");
}

void handle_add_user(int client_fd, char *username, char *password)
{
    if (username == NULL || password == NULL)
    {
        send_response(client_fd, "-ERR Faltan parámetros para ADD-USER\r\n");
        return;
    }
    char msg[128];
    snprintf(msg, sizeof(msg), "+OK Usuario %s agregado con pass %s\r\n", username, password);
    send_response(client_fd, msg);
}

void handle_delete_user(int client_fd, char *username)
{
    if (username == NULL)
    {
        send_response(client_fd, "-ERR Falta parámetro para DELETE-USER\r\n");
        return;
    }
    char msg[128];
    snprintf(msg, sizeof(msg), "+OK Usuario %s eliminado\r\n", username);
    send_response(client_fd, msg);
}

void handle_change_pass(int client_fd, char *old_pass, char *new_pass)
{
    if (old_pass == NULL || new_pass == NULL)
    {
        send_response(client_fd, "-ERR Faltan parámetros para CHANGE-PASS\r\n");
        return;
    }
    char msg[128];
    snprintf(msg, sizeof(msg), "+OK Contraseña cambiada de %s a %s\r\n", old_pass, new_pass);
    send_response(client_fd, msg);
}

void handle_help(int client_fd)
{
    send_response(client_fd,
                  "Comandos válidos:\r\n"
                  " - LIST\r\n"
                  " - ADD-USER <usuario> <clave>\r\n"
                  " - DELETE-USER <usuario>\r\n"
                  " - CHANGE-PASS <vieja> <nueva>\r\n"
                  " - STAT\r\n"
                  " - HELP\r\n"
                  " - QUIT\r\n");
}

void handle_stat(int client_fd)
{
    send_response(client_fd, "Hay 2 usuarios activos\r\n");
}

void handle_command(int client_fd, char *input)
{
    char *cmd = strtok(input, " ");
    char *arg1 = strtok(NULL, " ");
    char *arg2 = strtok(NULL, " ");

    if (!cmd)
        return;

    if (strcmp(cmd, "LIST") == 0)
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
        handle_change_pass(client_fd, arg1, arg2);
    }
    else if (strcmp(cmd, "HELP") == 0)
    {
        handle_help(client_fd);
    }
    else if (strcmp(cmd, "STAT") == 0)
    {
        handle_stat(client_fd);
    }
    else if (strcmp(cmd, "QUIT") == 0)
    {
        send_response(client_fd, "chau\r\n");
        close(client_fd);
        exit(0);
    }
    else
    {
        send_response(client_fd, "-ERR Comando no reconocido. Escriba HELP\r\n");
    }
}

int main()
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 1) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Servidor escuchando en el puerto %d...\n", PORT);

    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0)
    {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    printf("Cliente conectado.\n");

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read <= 0)
        {
            printf("Cliente desconectado.\n");
            break;
        }

        trim_newline(buffer);
        printf(">> %s\n", buffer);
        handle_command(client_fd, buffer);
    }

    close(client_fd);
    close(server_fd);
    return 0;
}
