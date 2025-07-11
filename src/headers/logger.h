#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *buffer;
    size_t length;
    size_t capacity;
} StringBuilder;


StringBuilder *sb_create();
void sb_free(StringBuilder *sb);
int sb_append(StringBuilder *sb, const char *str);
const char *sb_get_string(const StringBuilder *sb);

#endif 