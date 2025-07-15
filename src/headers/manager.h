#ifndef MANAGER_H
#define MANAGER_H

#include <selector.h>
#include <unistd.h>
#include <sys/socket.h>
#include <selector.h>
#include <stdlib.h>
#include <copy.h>
#include <stdio.h>
#include <string.h>
#include <selector.h>
#include <users.h>
#include <metrics.h>

/**
 * Función que se registra como handler para aceptar nuevas conexiones
 * al puerto de monitoreo (tipo POP3).
 *
 * Se encarga de aceptar el socket, inicializar el estado y registrar
 * el nuevo cliente con el selector.
 *
 * @param key Selector key con el fd del socket pasivo.
 */
void manager_passive_accept(struct selector_key *key);

#endif // MANAGER_H
