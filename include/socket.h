/*****************************************************************************
 * ARCHIVO: socket.h
 * DESCRIPCION: Interfaz de programacion de sockets para el servidor. 
 *
 * FECHA CREACION: 25 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/
#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <stdio.h>

int socket_init(char* port, int backlog);
int socket_accept(int sock_fd);

#endif /* __SOCKET_H__ */

