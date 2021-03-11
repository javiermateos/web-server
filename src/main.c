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
#include "tpool.h"


tpool_t* tm = NULL;

void signal_handler(int s);

int main(void)
{
    int sock_fd, new_fd; // Escucha en sock_fd, nueva conexion en new_fd
    socklen_t sin_size;
    struct sockaddr their_addr; // informacion de la direccion del cliente
    char s[INET_ADDRSTRLEN];
    struct sigaction sa;

    // Inicializacion del servidor
    sock_fd = init_server();

    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL)) {
        perror("sigaction");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &sa, NULL)) {
        perror("sigaction");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // Inicializacion del pool de threads
    tm = tpool_create(NUM_THREADS);
    if (!tm) {
        fprintf(stderr, "Error initializing thread pool...\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("server: esperando conexiones entrantes...\n");

    while (1) {
        sin_size = sizeof their_addr;
        // Aceptamos una nueva conexion
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
        printf("server: conexion entrante de %s\n", s);

        // Realizamos el trabajo en un hilo
        tpool_add_work(tm, server_thread, &new_fd);
    }
    
    return 0;
}

void signal_handler(int s)
{
    fprintf(stdout, "Senial %d recibida, esperando que finalizen los hilos...\n", s);
    tpool_destroy(tm);
    exit(EXIT_SUCCESS);
}
