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

/*******************************************************************************
 * FUNCION: int socket_init(char* port, int backlog)
 * ARGS_IN: char* port - Puerto del socket.
 *          int backlog - Numero de usuario maximo de la cola del socket.
 * DESCRIPCION: Crea e inicializa un socket del conexion para el servidor.
 * ARGS_OUT: int - Descriptor de fichero del socket.
 ******************************************************************************/
int socket_init(char* port, int backlog);

/*******************************************************************************
 * FUNCION: int socket_accept(int sock_fd)
 * ARGS_IN: int sock_fd - Descriptor de fichero del socket en el que escucha el
 *                        servidor.
 * DESCRIPCION: Acepta conexiones entrantes al servidor.
 * ARGS_OUT: int - Descriptor de fichero del socket para la conexion recibida.
 ******************************************************************************/
int socket_accept(int sock_fd);

/*******************************************************************************
 * FUNCION: int socket_send(int sock_fd, char* response_header, char*
 *                          response_body, int response_body_len)
 * ARGS_IN: int sock_fd - Descriptor de fichero del socket de la conexion con
 *                        el cliente.
 *          char* response_header - Cabecera del mensaje enviado.
 *          char* response_body - Cuerpo del mensaje enviado.
 *          int response_body_len - Longitud del cuerpo del mensaje.
 * DESCRIPCION: Acepta conexiones entrantes al servidor.
 * ARGS_OUT: int - 0 si se ha enviado todo correctamente o -1 si se produce
 *                 algun error.
 ******************************************************************************/
int socket_send(int sock_fd,
                char* response_header,
                char* response_body,
                int response_body_len);

#endif /* __SOCKET_H__ */
