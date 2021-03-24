/*****************************************************************************
 * ARCHIVO: server.c
 * DESCRIPCION: Implementacion de la interfaz de programacion de un servidor
 * mediante sockets.
 *
 * FECHA CREACION: 4 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/

#include <stdio.h> // fprintf
#include <stdlib.h> // NULL
#include <string.h> // memset

#include <sys/types.h>
#include <sys/socket.h> // getaddrinfo, socket, setsockopt, bind
#include <netdb.h> // addrinfo, getaddrinfo
#include <unistd.h> // close

#include "server.h"

int init_server(struct ini* config)
{
    int sock_fd; // Escucha en sock_fd
    struct addrinfo hints, *servinfo, *p;
    int optval = 1;
    int rv;
    char* port = NULL;
    int backlog;

    // Configuracion de la direccion del socket
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    port = ini_get_value(config, "inicializacion", "listen_port");

    rv = getaddrinfo(NULL, port, &hints, &servinfo);
    if (rv != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return EXIT_FAILURE;
    }

    // Loop a traves de todos los resultados y bindeamos el primero que podemos
    for (p = servinfo; p != NULL; p = p->ai_next) {
        sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock_fd == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) ==
            -1) {
            perror("server: setsockopt");
            return EXIT_FAILURE;
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
        return EXIT_FAILURE;
    }

    backlog = atoi(ini_get_value(config, "inicializacion", "max_clients"));

    if (listen(sock_fd, backlog) == -1) {
        perror("listen");
        return EXIT_SUCCESS;
    }

    return sock_fd;
}

void process_connection(int sock_fd)
{
    char *msg = "Hello World!\n";

    if (send(sock_fd, msg, strlen(msg), 0) < 1) {
        perror("server: send");
    }

    close(sock_fd);
}

void server_thread(void* args) {
    int sock_fd = *((int*)args);

    process_connection(sock_fd);
}
