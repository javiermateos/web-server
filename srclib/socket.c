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
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // Loop a traves de todos los resultados y bindeamos el primero que podemos
    for (p = servinfo; p != NULL; p = p->ai_next) {
        sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock_fd == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(
              sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1) {
            perror("server: setsockopt");
            return -1;
        }

        if (bind(sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sock_fd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (!p) {
        fprintf(stderr, "server: failed to bind\n");
        return -1;
    }

    if (listen(sock_fd, backlog) == -1) {
        perror("listen");
        return -1;
    }

    return sock_fd;
}

int socket_accept(int sock_fd)
{
    int new_fd;
    char s[INET_ADDRSTRLEN];
    socklen_t sin_size;
    struct sockaddr their_addr; // informacion de la direccion del cliente

    sin_size = sizeof their_addr;
    // Aceptamos una nueva conexion
    new_fd = accept(sock_fd, &their_addr, &sin_size);
    if (new_fd == -1) {
        perror("server: accept");
        return -1;
    }

    // Obtenemos la direccion IP del cliente
    inet_ntop(their_addr.sa_family,
              &(((struct sockaddr_in*)&their_addr)->sin_addr),
              s,
              sizeof s);
    fprintf(stdout, "server: conexion entrante de %s\n", s);

    return new_fd;
}
