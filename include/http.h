/******************************************************************************
 * ARCHIVO: http.h
 * DESCRIPCION: Interfaz de programacion para el protocolo HTTP/1.1
 *
 * FECHA CREACION: 4 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/
#ifndef __HTTP_H__
#define __HTTP_H__

#include <stdio.h>

/******************************************************************************
 * FUNCION: int http(int socket, char* server_root, char* server_signature)
 * ARGS_IN: int socket - identificador del hilo donde se escribe.
 *          char* server_root - ruta a los recursos del servidor.
 *          char* server_signature - nombre del servidor.
 * DESCRIPCION: establece la conexión entre el socket y el servidor para que se
 *              comuniquen mediante el protocolo http.
 * ARGS_OUT: int - devuelve 0 en caso de funcionar correctamente -1 en caso
 *                 contrario.
 *****************************************************************************/
int http(int socket, char* server_root, char* server_signature);

#endif /* __HTTP_H__ */