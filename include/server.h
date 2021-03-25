/*****************************************************************************
 * ARCHIVO: server.h
 * DESCRIPCION: Interfaz de programacion para un servidor mediante sockets. 
 *
 * FECHA CREACION: 4 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/
#ifndef __SERVER_H__
#define __SERVER_H__

#include <stdio.h>
#include <stdbool.h>

typedef struct server server_t;

int server_init(bool daemon, int num_threads, char* port, int backlog);
void server_close();
void server_run();

#endif /* __SERVER_H__ */

