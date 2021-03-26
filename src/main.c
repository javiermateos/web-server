/*****************************************************************************
 * ARCHIVO: main.c
 * DESCRIPCION: Archivo principal que ejecuta la lógica del servidor.
 *
 * FECHA CREACION: 4 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/

#include <fcntl.h>   // open
#include <signal.h>  // pthread_sigmask
#include <stdbool.h> // bool
#include <stdio.h>
#include <sys/resource.h> // getrlimit
#include <sys/socket.h>   // send
#include <sys/stat.h>     // umask
#include <sys/syslog.h>
#include <syslog.h> // openlog
#include <unistd.h> // close

#include "iniparser.h"
#include "socket.h"
#include "tpool.h"

int sock_fd;
tpool_t* tm;
bool daemon_proc;
bool debug;
struct config_s {
    struct read_ini* ri;
    struct ini* conf;
} config;

static void signal_handler();
static void thread_routine(void* args);
static void daemon_process();
static void logger(int priority, char* message);

int main(void)
{
    int num_threads, backlog;
    char* port = NULL;
    int new_fd;
    struct sigaction sa;

    config.conf = read_ini(&config.ri, "server.ini");
    if (!config.conf) {
        fprintf(stderr, "Error leyendo el fichero de configuracion...\n");
        exit(EXIT_FAILURE);
    }

    backlog = atoi(ini_get_value(config.conf, "inicializacion", "max_clients"));
    port = ini_get_value(config.conf, "inicializacion", "listen_port");
    num_threads =
      atoi(ini_get_value(config.conf, "inicializacion", "num_threads"));
    daemon_proc = atoi(ini_get_value(config.conf, "inicializacion", "daemon"));
    debug = atoi(ini_get_value(config.conf, "inicializacion", "debug"));

    if (daemon_proc) {
        fprintf(stdout, "Ejecutando servidor como proceso daemon...\n");
        daemon_process();
        // Inicializamos el fichero de logs
        if (debug) {
            setlogmask(LOG_UPTO(LOG_DEBUG));
        } else {
            setlogmask(LOG_UPTO(LOG_INFO));
        }
        openlog("SERVER", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);
    }

    logger(LOG_DEBUG, "Iniciando el socket...\n");
    sock_fd = socket_init(port, backlog);
    if (sock_fd == -1) {
        logger(LOG_ERR, "Error inicializando el socket...\n");
        exit(EXIT_FAILURE);
    }

    logger(LOG_DEBUG, "Iniciando el pool de hilos...\n");
    tm = tpool_create(num_threads);
    if (!tm) {
        logger(LOG_ERR, "Error inicializando el pool de hilos...\n");
        exit(EXIT_FAILURE);
    }

    // Establecemos el tratamiento de las seniales de terminacion del programa
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL)) {
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &sa, NULL)) {
        exit(EXIT_FAILURE);
    }

    // No necesitamos mas parametros de configuracion
    destroy_ini(config.conf);
    cleanup_readini(config.ri);

    logger(LOG_INFO, "Servidor listo para recibir conexiones...\n");

    do {
        new_fd = socket_accept(sock_fd);
        if (new_fd == -1) {
            continue;
        }
        logger(LOG_INFO, "Conexion entrante recibida...\n");
        // Bloqueamos las seniales mientras se introduce el trabajo en la cola
        pthread_sigmask(SIG_BLOCK, &sa.sa_mask, NULL);
        // Insertamos el trabajo en la cola de trabajos del pool de hilos
        if (!tpool_add_work(tm, thread_routine, &new_fd)) {
            logger(LOG_ERR, "Conexion entrante no procesada...\n");
            close(new_fd);
        }
        // Desbloqueamos las seniales
        pthread_sigmask(SIG_UNBLOCK, &sa.sa_mask, NULL);
    } while (1);

    exit(EXIT_SUCCESS);
}

static void signal_handler()
{
    logger(LOG_DEBUG,
           "Senial recibida, esperando a que finalicen los hilos...\n");
    close(sock_fd);
    tpool_destroy(tm);
    exit(EXIT_SUCCESS);
}

static void thread_routine(void* args)
{
    int fd = *((int*)args);
    char* msg = "Hello World!\n";

    if (send(fd, msg, strlen(msg), 0) < 1) {
        logger(LOG_ERR, "Error enviando el mensaje al cliente...\n");
    }

    close(fd);
}

static void daemon_process()
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
        exit(EXIT_FAILURE);
    } else if (pid != 0) {
        destroy_ini(config.conf);
        cleanup_readini(config.ri);
        exit(EXIT_SUCCESS);
    }

    // Creamos una nueva sesion en la que el hijo el el lider
    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }

    // Aseguramos que no se asignen TTYs
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        exit(EXIT_FAILURE);
    }

    // Prevenimos que pueda adquirir una TTY
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    } else if (pid != 0) {
        destroy_ini(config.conf);
        cleanup_readini(config.ri);
        exit(EXIT_SUCCESS);
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
    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        exit(EXIT_FAILURE);
    }
}

static void logger(int priority, char* message)
{
    if (daemon_proc) {
        syslog(priority, "%s", message);
    } else {
        switch (priority) {
            case LOG_INFO:
                fprintf(stdout, "[Server] [LOG_INFO]: %s", message);
                break;
            case LOG_ERR:
                fprintf(stderr, "[Server] [LOG_ERR]: %s", message);
                break;
            case LOG_DEBUG:
                if (debug) {
                    fprintf(stdout, "[Server] [LOG_DEBUG]: %s", message);
                }
        }
    }
}
