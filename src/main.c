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
#include <unistd.h>     // fork, close

#include "iniparser.h"
#include "server.h"
#include "tpool.h"

tpool_t* tm = NULL;

void signal_handler();

int main(void)
{
    int sock_fd, new_fd; // Escucha en sock_fd, nueva conexion en new_fd
    int num_threads;
    char s[INET_ADDRSTRLEN];
    socklen_t sin_size;
    struct sockaddr their_addr; // informacion de la direccion del cliente
    struct sigaction sa;
    struct read_ini* ri = NULL;
    struct ini* config = NULL;

    // Obtenemos los parametros de configuracion del servidor
    config = read_ini(&ri, "server.ini");

    // Inicializacion del servidor
    sock_fd = init_server(config);

    // Inicializacion del pool de threads
    num_threads = atoi(ini_get_value(config, "inicializacion", "num_threads"));
    tm = tpool_create(num_threads);
    if (!tm) {
        fprintf(stderr, "Error initializing thread pool...\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // No necesitamos mas parametros de configuracion
    destroy_ini(config);
    cleanup_readini(ri);

    // Establecemos el tratamiento de las seniales de terminacion del programa
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

    tpool_destroy(tm);

    exit(EXIT_SUCCESS);
}

void signal_handler()
{
    fprintf(
      stdout, "Finalizando programa servidor, esperando que finalizen los hilos...\n");
    tpool_destroy(tm);
    exit(EXIT_SUCCESS);
}
