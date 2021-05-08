/*****************************************************************************
 * ARCHIVO: http.h
 * DESCRIPCION: Interfaz de programacion para el protocolo HTTP/1.1
 *
 * FECHA CREACION: 4 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/
#ifndef __HTTP_H__
#define __HTTP_H__

#include <stdio.h>

int http(int socket, char* server_root, char* server_signature);

#endif /* __HTTP_H__ */
