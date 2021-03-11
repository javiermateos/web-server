/*****************************************************************************
 * ARCHIVO: server.h
 * DESCRIPCION: Interfaz de programacion para un servidor mediante sockets. 
 *
 * FECHA CREACION: 4 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/
#ifndef __SERVER_H__
#define __SERVER_H__

#define PORT "3490" // Puerto de escucha del servidor
#define BACKLOG 10 // Numero de conexiones pendientes que la cola mantiene 
#define NUM_THREADS 10

int init_server();
void server_thread(void* arg);

#endif /* __SERVER_H__ */

