#ifndef USERS_H
#define USERS_H

#define MAX_LENGTH 255
#define ADMIN 1
#define COMMONER 0

typedef struct{
    char name[MAX_LENGTH+1];
    char pass[MAX_LENGTH+1];
    int status;
}user;

int newUser(const char* name, const char* pass);

void changeStatus(const char* name, int newStatus);

int deleteUser(const char* name);

int initUsers();

int closeUsers();

int userExists(const char* name);

#endif
