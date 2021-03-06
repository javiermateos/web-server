/*****************************************************************************
 * ARCHIVO: main.c
 * DESCRIPCION: Archivo principal que ejecuta la lógica del servidor.
 *
 * FECHA CREACION: 4 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/
#include <arpa/inet.h>  // sockaddr, sockaddr_storage, inet_ntop
#include <errno.h>      // errno
#include <pthread.h>    // pthread_t, pthread_create
#include <signal.h>     // sigaction, sigemptyset
#include <stdio.h>      // printf, perror
#include <stdlib.h>     // exit
#include <sys/socket.h> // getaddrinfo, socket, setsockopt, bind
#include <sys/wait.h> //waitpid
#include <unistd.h>   // fork, close

#include "server.h"

int main(void)
{
    int sock_fd, new_fd; // Escucha en sock_fd, nueva conexion en new_fd
    socklen_t sin_size;
    struct sockaddr their_addr; // informacion de la direccion del cliente
    char s[INET_ADDRSTRLEN];
    pthread_t main_thread;
    thread_args_t* args = NULL;

    // Inicializacion del servidor
    sock_fd = init_server();

    printf("server: esperando conexiones entrantes...\n");

    while (1) {
        sin_size = sizeof their_addr;
        new_fd = accept(sock_fd, &their_addr, &sin_size);
        if (new_fd == -1) {
            perror("server: accept");
            continue;
        }

        // Obtenemos la direccion IP del cliente
        inet_ntop(their_addr.sa_family,
                  &(((struct sockaddr_in*)&their_addr)->sin_addr),
                  s,
                  sizeof s);
        printf("server: conexion de %s\n", s);

        args = (thread_args_t*)malloc(sizeof(thread_args_t));
        if (!args) {
            fprintf(stderr, "Error inicializando los argumentos del hilo..\n");
            exit(EXIT_FAILURE);
        }

        args->sock_fd = new_fd;
        pthread_create(&main_thread, NULL, server_thread, args);

        pthread_detach(main_thread);
    }

    return 0;
}
