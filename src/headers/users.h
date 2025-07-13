#ifndef USERS_H
#define USERS_H

#define MAX_LENGTH 255
#define ADMIN 1
#define COMMONER 0

#include "logger.h"

typedef struct
{
    char name[MAX_LENGTH + 1];
    char pass[MAX_LENGTH + 1];
    int status;
    StringBuilder *access;
} Tuser;

Tuser *users; // lista de usuarios

int new_user(const char *name, const char *pass);

void change_status(const char *name, int newStatus);

int change_password(const char *name, const char *old, const char *new);

int delete_user(const char *name);

int init_users();

int close_users();

int user_login(const char *name, const char *password);

int user_exists(const char *name);

const Tuser *get_users();

unsigned int get_user_count();

#endif
