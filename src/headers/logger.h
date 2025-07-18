
#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "request_parser.h"

typedef struct {
    char *buffer;
    size_t length;
    size_t capacity;
} StringBuilder;

typedef enum {INFO=0, WARNING, ERROR, FATAL} LOG_TYPE;
const char *type_to_string(LOG_TYPE type);
char* int_to_string(int num);

StringBuilder *sb_create();
void sb_free(StringBuilder *sb);
int sb_append(StringBuilder *sb, const char *str);
const char *sb_get_string(const StringBuilder *sb);

void create_logs_sb();
int logs_append(const char *str) ;
void log_free();
const char *get_logs(); 
void our_log(LOG_TYPE type, const char* msg);
void set_log_type(LOG_TYPE new);
void sb_remove_last_line(StringBuilder *sb);
const char* status_to_string(connection_status status);
#endif
