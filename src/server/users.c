#include <users.h>
#include <stdbool.h>
#include <stdio.h>  /* for printf */
#include <stdlib.h> /* for exit */
#include <limits.h> /* LONG_MIN et al */
#include <string.h> /* memset */
#include <errno.h>
#include <getopt.h>
#include "args.h"

static user *users; // lista de usuarios
static unsigned int cantUsers, admins;

int newUser(const char *name, const char *pass)
{

    // Chequear que name y pass no estén vacíos
    if (name == NULL || name[0] == '\0') {
        fprintf(stderr, "Username must have at least one character.\n");
        return -1;
    }
    if (pass == NULL || pass[0] == '\0') {
        fprintf(stderr, "Password must have at least one character.\n");
        return -1;
    }
    
    if (userExists(name) >= 0)
    {
        fprintf(stderr, "Username is already in use, please choose another name.\n");
        return -1;
    }
    if (cantUsers >= MAX_USERS)
    {
        fprintf(stderr, "You have reached max amount of users, we cant create %s\n", name);
        return -1;
    }

    memset(users[cantUsers].name, 0, MAX_LENGTH + 1);
    strncpy(users[cantUsers].name, name, MAX_LENGTH);
    memset(users[cantUsers].pass, 0, MAX_LENGTH + 1);
    strncpy(users[cantUsers].pass, pass, MAX_LENGTH);

    users[cantUsers].status = COMMONER;
    cantUsers++;

    fprintf(stdout, "Created user %s\n", name);
    return 0;
}

void changeStatus(const char *name, int newStatus)
{

    int index = userExists(name);

    if (index < 0)
    {
        fprintf(stderr, "This user does not exist. C\n");
        return;
    }

    if (users[index].status == newStatus)
    {
        return;
    }

    // TODO chequeo de status del q pide la acción
    // preguntarle a las chicas si sus protocolos guarda q usuario esta iniciado sesión

    users[index].status = newStatus;
    if (newStatus == ADMIN)
    {
        admins++;
    }
    if (newStatus == COMMONER)
    {
        admins--;
    }
}

int changePassword(const char *name, const char *old, const char *new){
    
    int index = userExists(name);
    if (index == -1)
    {
        fprintf(stderr, "This username does not exist.\n");
        return -1;
    }

    // Check pass
    if (strcmp(users[index].pass, old) == 0)
    {
        strncpy(users[index].pass, new, MAX_LENGTH);
        users[index].pass[MAX_LENGTH] = '\0';
        return 0;
    }

    fprintf(stderr, "The password is incorrect.\n");
    return -1;
}


int deleteUser(const char *name)
{
    fprintf(stderr, "en delete\n");
    int index = userExists(name);

    if (index < 0)
    {
        fprintf(stderr, "This user does not exist. D\n");
        return -1;
    }

    if (users[index].status == ADMIN)
    {
        if (admins == 1)
        { // solo queda uno
            fprintf(stderr, "We cant allow you to delete %s, its the only user with ADMIN powers.\n", name);
            return -1;
        }
        admins--;
    }

    for (int i = index; i < cantUsers - 1; i++)
    { // piso el user borrado
        users[i] = users[i + 1];
    }

    cantUsers--;
    return 0;
}

int initUsers()
{
    users = NULL;
    cantUsers = 0;

    users = malloc(MAX_USERS * sizeof(user));
    if (users == NULL)
    {
        fprintf(stderr, "Error in malloc for 'users' array\n");
        return -1;
    }

    if (admins == 0)
    {
        fprintf(stderr, "No admins yet, creating admin with name: admin and password: admin\n");
        newUser("admin", "admin");
        changeStatus("admin", ADMIN);
    }

    return 0;
}

int closeUsers()
{
    free(users);
    return 0;
}

int userLogin(const char *name, const char *password)
{
    int index = userExists(name);
    if (index == -1)
    {
        return -1;
    }

    // Check pass
    if (strcmp(users[index].pass, password) == 0)
    {
        return 0;
    }

    return -1;
}

int userExists(const char *name)
{
    for (int i = 0; i < cantUsers; i++)
    {
        if (strcmp(users[i].name, name) == 0)
        {
            return i; // Lo encontramos en el indice i
        }
    }
    return -1;
}

const user *getUsers()
{
    return users;
}

unsigned int getUserCount()
{
    return cantUsers;
}