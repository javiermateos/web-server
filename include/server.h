/*****************************************************************************
 * ARCHIVO: server.h
 * DESCRIPCION: Interfaz de programacion para un servidor mediante sockets. 
 *
 * FECHA CREACION: 4 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/
#ifndef __SERVER_H__
#define __SERVER_H__

#include "iniparser.h"

int init_server(struct ini* config);
void server_thread(void* arg);
void server_daemon();

#endif /* __SERVER_H__ */

