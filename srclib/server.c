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

#include <sys/socket.h> // getaddrinfo, socket, setsockopt, bind
#include <sys/stat.h> // umask
#include <sys/resource.h> // getrlimit
#include <netdb.h> // addrinfo, getaddrinfo
#include <sys/syslog.h>
#include <unistd.h> // close
#include <signal.h> // sigaction
#include <fcntl.h> // open
#include <syslog.h> // openlog

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

void server_thread(void* args) 
{
    int sock_fd = *((int*)args);

    process_connection(sock_fd);
}

void server_daemon()
{
    int fd0, fd1, fd2;
    unsigned long i;
    pid_t pid;
    struct rlimit rl;
    struct sigaction sa;

    // Cambiamos la mascara para los permisos de los archivos
    umask(0);

    // Obtenemos el numero maximo de descriptores de ficheros
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        exit(EXIT_FAILURE);
    }

    // Convertimos el proceso en el lider de la sesion para perder el control
    // por parte de la TTY
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid != 0) {
        // Proceso padre
        exit(EXIT_SUCCESS);
    }

    // Creamos una nueva sesion en la que el hijo el el lider
    if (setsid() < 0) {
        perror("setsid");
        exit(EXIT_FAILURE);
    }

    // Aseguramos que no se asignen TTYs
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Prevenimos que pueda adquirir una TTY
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid != 0) {
        // Proceso padre
        exit(EXIT_SUCCESS);
    }

    // Cambiamos el directorio de trabajo al directorio root para que
    // no haya problemas al hacer el unmount de los sistemas de ficheros.
    if (chdir("/") < 0) {
        perror("chdir");
    }

    // Cerramos todos los descriptores de ficheros.
    if (rl.rlim_max == RLIM_INFINITY) {
        rl.rlim_max = 1024;
    }
    for (i = 0; i < rl.rlim_max; i++) {
        close(i);
    }

    // Asignamos los descriptores estandar a dev/null por si alguna rutina
    // trata de emplearlos.
    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);

    // Inicializamos el fichero de logs
    openlog("SERVER", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);
    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        syslog(LOG_ERR, "Descriptores de ficheros inesperados %d %d %d", fd0, fd1, fd2);
        exit(EXIT_FAILURE);
    }
}
