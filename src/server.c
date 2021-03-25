/*****************************************************************************
 * ARCHIVO: server.c
 * DESCRIPCION: Implementacion de la interfaz de programacion de un servidor
 * mediante sockets.
 *
 * FECHA CREACION: 4 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/

#include <signal.h>     // sigaction, sigemptyset
#include <stdio.h>      // fprintf
#include <stdlib.h>     // NULL
#include <string.h>     // strlen
#include <sys/socket.h> // send
#include <unistd.h>     // close

#include "server.h"
#include "socket.h"
#include "tpool.h"

struct server {
    tpool_t* tm; // Pool de hilos del servidor
    int sock_fd;
    bool daemon; // Indica que el servidor debe ser un proceso "daemon"
};

static server_t* server; // Variable global que representa al servidor

// FUNCIONES PRIVADAS
static void server_thread_routine(void* args);
static void server_signal_handler(int s);

int server_init(bool daemon, int num_threads, char* port, int backlog)
{
    struct sigaction sa;

    server = (server_t*)malloc(sizeof(server_t));
    if (!server) {
        fprintf(stderr, "Error reservando memoria para el servidor...\n");
        return 1;
    }

    // Inicializacion del pool de threads
    server->tm = tpool_create(num_threads);
    if (!server->tm) {
        fprintf(stderr, "Error inicializando el pool de hilos...\n");
        return 1;
    }

    // Inicializacion del socket del servidor
    server->sock_fd = socket_init(port, backlog);
    if (server->sock_fd == -1) {
        fprintf(stderr, "Error inicializando el socket...\n");
        return 1;
    }

    server->daemon = daemon;

    // Establecemos el tratamiento de las seniales de terminacion del programa
    sa.sa_handler = server_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL)) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &sa, NULL)) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    return 0;
}

void server_close()
{
    if (!server) {
        return;
    }

    // Liberamos los recursos reservados para el servidor
    tpool_destroy(server->tm);
    close(server->sock_fd);
    free(server);
}

void server_run()
{
    int new_fd;
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);

    printf("Servidor: esperando conexiones entrantes...\n");

    do {
        new_fd = socket_accept(server->sock_fd);
        if (new_fd == -1) {
            // TODO: Loguear informacion
            continue;
        }
        // Bloqueamos las seniales mientras se introduce el trabajo en la cola
        pthread_sigmask(SIG_BLOCK, &set, NULL);
        // Insertamos el trabajo en la cola de trabajos del pool de hilos
        tpool_add_work(server->tm, server_thread_routine, &new_fd);
        // Desbloqueamos las seniales
        pthread_sigmask(SIG_UNBLOCK, &set, NULL);
    } while (1);
}

static void server_thread_routine(void* args)
{
    int sock_fd = *((int*)args);
    char* msg = "Hello World!\n";

    if (send(sock_fd, msg, strlen(msg), 0) < 1) {
        perror("server: send");
    }

    close(sock_fd);
}

static void server_signal_handler(int s)
{
    fprintf(
      stdout, "Senial %d recibida, esperando que finalizen los hilos...\n", s);
    server_close();
    exit(EXIT_SUCCESS);
}
