#include "logger.h"


#define SB_INITIAL_CAPACITY 64
#define NO_LOGS_PLS 0 //Pasar a 1 si no se quiere ningun tipo de log


static unsigned int current_type = INFO;
StringBuilder *log_sb;


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


void create_logs_sb() {
    //inicializamos el string de logs
    log_sb = malloc(sizeof(StringBuilder));
    if (!log_sb) return ;


    log_sb->buffer = malloc(SB_INITIAL_CAPACITY);
    if (!log_sb->buffer) {
        free(log_sb);
        return ;
    }
    log_sb->length = 0;
    log_sb->capacity = SB_INITIAL_CAPACITY;
    log_sb->buffer[0] = '\0';  // string vacío válido
}


int logs_append(const char *str ) {
    size_t str_len = strlen(str);
    size_t needed = log_sb->length + str_len + 1;


    // Si no alcanza, hacemos realloc
    if (needed > log_sb->capacity) {
        size_t new_capacity = log_sb->capacity * 2;
        while (new_capacity < needed)
            new_capacity *= 2;


        char *new_buffer = realloc(log_sb->buffer, new_capacity);
        if (!new_buffer) return -1;


        log_sb->buffer = new_buffer;
        log_sb->capacity = new_capacity;
    }


    memcpy(log_sb->buffer + log_sb->length, str, str_len + 1);
    log_sb->length += str_len;


    return 0;
}


const char *get_logs() {
    return log_sb->buffer;
}


void log_free() {
    if (log_sb) {
        free(log_sb->buffer);
        free(log_sb);
    }
}


void our_log(LOG_TYPE type, const char* msg){
   
    if(!NO_LOGS_PLS){
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", tm_info);
        const char* type_str = type_to_string(type);


        if(type >= current_type){
            printf("[%s] [%s] [%s]\n", timestamp, type_str, msg);
        }
       
        logs_append(timestamp);
        logs_append(" ");
        logs_append(type_str);
        logs_append(" ");
        logs_append(msg);
        logs_append("\n");
    }
}


void set_log_type(LOG_TYPE new){
    if(new>=INFO && new<=FATAL){
        current_type=new;
    }
}


const char* type_to_string(LOG_TYPE type) {
    switch (type) {
        case INFO:    return "INFO";
        case WARNING: return "WARNING";
        case ERROR:   return "ERROR";
        case FATAL:   return "FATAL";
        default:      return "UNKNOWN";
    }
}


char* int_to_string(int num) {
    int isNegative = 0;
    int temp = num;
    int len = 0;


    if (num == 0) {
        char *zero = malloc(2);
        if (!zero) return NULL;
        zero[0] = '0';
        zero[1] = '\0';
        return zero;
    }
    if (num < 0) {
        isNegative = 1;
        temp = -temp;
    }
    int n = temp;
    while (n != 0) {
        n /= 10;
        len++;
    }


    char *str = malloc(len + isNegative + 1);  
    if (!str) return NULL;


    int i = len + isNegative - 1;
    str[i + 1] = '\0';


    while (temp != 0) {
        str[i--] = (temp % 10) + '0';
        temp /= 10;
    }


    if (isNegative)
        str[0] = '-';


    return str;
}


void sb_remove_last_line(StringBuilder *sb) {
    if (sb == NULL || sb->length == 0) return;


    // Busca el último '\n' (que separa la última línea válida del contenido a borrar)
    char *last_newline = NULL;
    for (size_t i = sb->length; i > 0; i--) {
        if (sb->buffer[i - 1] == '\n') {
            last_newline = &sb->buffer[i - 1];
            break;
        }
    }


    if (last_newline != NULL) {
        size_t new_length = last_newline - sb->buffer;
        sb->buffer[new_length] = '\0';
        sb->length = new_length;
    } else {
        // No hay '\n', entonces se borra todo
        sb->buffer[0] = '\0';
        sb->length = 0;
    }
}


const char* status_to_string(connection_status status) {
    switch (status) {
        case CON_SUCCEEDED:
            return "Succeeded";
        case CON_SERVER_FAILURE:
            return "General server failure";
        case CON_NOT_ALLOWED:
            return "Connection not allowed by ruleset";
        case CON_NET_UNREACHABLE:
            return "Network unreachable";
        case CON_HOST_UNREACHABLE:
            return "Host unreachable";
        case CON_REFUSED:
            return "Connection refused";
        case CON_TTL_EXPIRED:
            return "TTL expired";
        case CON_COMMAND_NOT_SUPPORTED:
            return "Command not supported";
        case CON_ATYPE_NOT_SUPPORTED:
            return "Address type not supported";
        case CON_OTHER:
            return "Other failure";
        default:
            return "Unknown status";
    }
}
