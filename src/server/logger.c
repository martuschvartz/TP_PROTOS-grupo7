#include "logger.h"

#define SB_INITIAL_CAPACITY 64

StringBuilder *sb_create() {
    StringBuilder *sb = malloc(sizeof(StringBuilder));
    if (!sb) return NULL;

    sb->buffer = malloc(SB_INITIAL_CAPACITY);
    if (!sb->buffer) {
        free(sb);
        return NULL;
    }

    sb->length = 0;
    sb->capacity = SB_INITIAL_CAPACITY;
    sb->buffer[0] = '\0';  // string vacío válido
    return sb;
}

void sb_free(StringBuilder *sb) {
    if (sb) {
        free(sb->buffer);
        free(sb);
    }
}

int sb_append(StringBuilder *sb, const char *str) {
    size_t str_len = strlen(str);
    size_t needed = sb->length + str_len + 1; // +1 para el '\0'

    // Si no alcanza, hacemos realloc
    if (needed > sb->capacity) {
        size_t new_capacity = sb->capacity * 2;
        while (new_capacity < needed)
            new_capacity *= 2;

        char *new_buffer = realloc(sb->buffer, new_capacity);
        if (!new_buffer) return -1;

        sb->buffer = new_buffer;
        sb->capacity = new_capacity;
    }

    memcpy(sb->buffer + sb->length, str, str_len + 1); // copiamos con '\0'
    sb->length += str_len;

    return 0;
}

const char *sb_get_string(const StringBuilder *sb) {
    return sb->buffer;
}
