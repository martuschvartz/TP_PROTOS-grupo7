#include <users.h>
#include <stdbool.h>
#include <stdio.h>  /* for printf */
#include <stdlib.h> /* for exit */
#include <limits.h> /* LONG_MIN et al */
#include <string.h> /* memset */
#include <errno.h>
#include <getopt.h>
#include "args.h"

static unsigned int cantUsers, admins;

int new_user(const char *name, const char *pass)
{

    // Chequear que name y pass no estén vacíos
    if (name == NULL || name[0] == '\0') {
        our_log(WARNING, "Username must have at least one character.");
        return -1;
    }
    if (pass == NULL || pass[0] == '\0') {
        our_log(WARNING, "Password must have at least one character.");
        return -1;
    }
    
    if (user_exists(name) >= 0)
    {
        our_log(WARNING, "Username is already in use, please choose another name.");
        return -1;
    }

    if (cantUsers >= MAX_USERS)
    {
        StringBuilder *sb = sb_create();
        sb_append(sb, "You have reached max amount of users, we cant create ");
        sb_append(sb, name);
        our_log(WARNING, sb_get_string(sb));
        sb_free(sb);

        return -1;
    }

    memset(users[cantUsers].name, 0, MAX_LENGTH + 1);
    strncpy(users[cantUsers].name, name, MAX_LENGTH);
    memset(users[cantUsers].pass, 0, MAX_LENGTH + 1);
    strncpy(users[cantUsers].pass, pass, MAX_LENGTH);
    StringBuilder *user_access = sb_create();
    users[cantUsers].access = user_access;
    users[cantUsers].status = COMMONER;
    cantUsers++;

    StringBuilder *sb = sb_create();
    sb_append(sb, "Created user ");
    sb_append(sb, name);
    our_log(INFO, sb_get_string(sb));
    sb_free(sb);
    
    return 0;
}

void change_status(const char *name, int newStatus)
{

    int index = user_exists(name);

    if (index < 0)
    {
        StringBuilder *sb = sb_create();
        sb_append(sb, "User ");
        sb_append(sb, name);
        sb_append(sb, ", does not exist.\n");
        our_log(WARNING, sb_get_string(sb));
        sb_free(sb);
        return;
    }

    if (users[index].status == newStatus)
    {
        return;
    }

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

int change_password(const char *name, const char *old, const char *new){
    
    int index = user_exists(name);
    if (index == -1)
    {
        StringBuilder *sb = sb_create();
        sb_append(sb, "User ");
        sb_append(sb, name);
        sb_append(sb, ", does not exist.\n");
        our_log(WARNING, sb_get_string(sb));
        sb_free(sb);        
        return -1;
    }

    // Check pass
    if (strcmp(users[index].pass, old) == 0)
    {
        strncpy(users[index].pass, new, MAX_LENGTH);
        users[index].pass[MAX_LENGTH] = '\0';
        return 0;
    }

    our_log(ERROR, "The password is incorrect.\n");
    return -1;
}


int delete_user(const char *name)
{
    int index = user_exists(name);

    if (index < 0)
    {
        StringBuilder *sb = sb_create();
        sb_append(sb, "User ");
        sb_append(sb, name);
        sb_append(sb, ", does not exist.\n");
        our_log(WARNING, sb_get_string(sb));
        sb_free(sb);        
        return -1;
    }

    if (users[index].status == ADMIN)
    {
        if (admins == 1)
        { // solo queda uno
             StringBuilder *sb = sb_create();
            sb_append(sb, "We cant allow you to delete ");
            sb_append(sb, name);
            sb_append(sb, ", its the only user with ADMIN powers.\n");
            our_log(ERROR, sb_get_string(sb));
            sb_free(sb);
            return -1;
        }
        admins--;
    }

    //le hago el free correspondiente a su sb
    sb_free(users[index].access);

    for (int i = index; i < cantUsers - 1; i++)
    { // piso el user borrado
        users[i] = users[i + 1];
    }

    cantUsers--;
    return 0;
}

int init_users()
{
    users = malloc(MAX_USERS * sizeof(Tuser));
    if (users == NULL)
    {
        our_log(ERROR, "Error in malloc for 'users' array");
        return -1;
    }

    if (admins == 0)
    {
        our_log(INFO, "No admins yet, creating admin with name: admin and password: admin");
        new_user("admin", "admin");
        change_status("admin", ADMIN);
    }

    return 0;
}

int close_users()
{
    //en el caso de que queden users, libero sus access
    for(int i=0; i<cantUsers; i++){
        if (users[i].access != NULL) {
            sb_free(users[i].access);
        }
    }

    free(users);
    return 0;
}

int user_login(const char *name, const char *password)
{
    int index = user_exists(name);
    if (index < 0)
    {
        StringBuilder *sb = sb_create();
        sb_append(sb, "User ");
        sb_append(sb, name);
        sb_append(sb, ", does not exist.\n");
        our_log(WARNING, sb_get_string(sb));
        sb_free(sb);        
        return -1;
    }

    // Check pass
    if (strcmp(users[index].pass, password) == 0)
    {
        StringBuilder *sb = sb_create();
        sb_append(sb, "Wrong password");
        our_log(WARNING, sb_get_string(sb));
        sb_free(sb); 
        return 0;
    }

    return -1;
}

int user_exists(const char *name)
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

const Tuser *get_users()
{
    return users;
}

unsigned int get_user_count()
{
    return cantUsers;
}

StringBuilder* get_access(const char* name){
    int index = user_exists(name);

    if (index < 0)
    {
        StringBuilder *sb = sb_create();
        sb_append(sb, "User ");
        sb_append(sb, name);
        sb_append(sb, ", does not exist.\n");
        our_log(WARNING, sb_get_string(sb));
        sb_free(sb);        
        return -1;
    }

    return users[index].access;
}