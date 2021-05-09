/*****************************************************************************
 * ARCHIVO: socket.c
 * DESCRIPCION: Implementacion de la interfaz de programacion de sockets para
 * el servidor.
 *
 * FECHA CREACION: 25 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/
#include <arpa/inet.h>  // inet_ntop
#include <netdb.h>      // addrinfo, getaddrinfo
#include <string.h>     // memset
#include <sys/socket.h> // getaddrinfo, socket, setsockopt, bind, listen, accept
#include <unistd.h>     // close

#include "socket.h"

int socket_init(char* port, int backlog)
{
    int sock_fd; // Escucha en sock_fd
    struct addrinfo hints, *servinfo, *p;
    int optval = 1;
    int rv;

    // Configuracion de la direccion del socket
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    rv = getaddrinfo(NULL, port, &hints, &servinfo);
    if (rv != 0) {
        return -1;
    }

    // Loop a traves de todos los resultados y bindeamos el primero que podemos
    for (p = servinfo; p != NULL; p = p->ai_next) {
        sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock_fd == -1) {
            continue;
        }

        if (setsockopt(
              sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1) {
            return -1;
        }

        if (bind(sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sock_fd);
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (!p) {
        return -1;
    }

    if (listen(sock_fd, backlog) == -1) {
        return -1;
    }

    return sock_fd;
}

int socket_accept(int sock_fd)
{
    int new_fd;
    socklen_t sin_size;
    struct sockaddr their_addr; // informacion de la direccion del cliente

    sin_size = sizeof their_addr;
    // Aceptamos una nueva conexion
    new_fd = accept(sock_fd, &their_addr, &sin_size);
    if (new_fd == -1) {
        return -1;
    }

    return new_fd;
}

int socket_send(int sock_fd,
                char* response_header,
                char* response_body,
                int response_body_len)
{
    ssize_t bytes;
    ssize_t offset;

    offset = 0;
    do {
        // Enviamos la cabecera de la respuesta
        bytes = send(sock_fd,
                     response_header + offset,
                     strlen(response_header) - offset,
                     0);
        if (bytes == -1) {
            return -1;
        }
        offset += bytes;
    } while (offset < (ssize_t)strlen(response_header));
    offset = 0;
    do {
        // Enviamos el cuerpo de la cabecera
        bytes =
          send(sock_fd, response_body + offset, response_body_len - offset, 0);
        if (bytes == -1) {
            // TODO: Revisar error ya que la cabecera ha sido enviada
            return -1;
        }
        offset += bytes;
    } while (offset < response_body_len);

    return 0;
}
