/*****************************************************************************
 * ARCHIVO: main.c
 * DESCRIPCION: Archivo principal que ejecuta la lógica del servidor.
 *
 * FECHA CREACION: 4 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/
#include <errno.h>  // errno
#include <stdio.h>  // printf, perror
#include <stdlib.h> // exit

#include "iniparser.h"
#include "server.h"

int main(void)
{
    int num_threads, backlog, daemon;
    char* port = NULL;
    struct read_ini* ri = NULL;
    struct ini* config = NULL;

    // Obtenemos los parametros de configuracion del servidor
    config = read_ini(&ri, "server.ini");
    if (!config) {
        fprintf(stderr, "Error leyendo el fichero de configuracion...\n");
        exit(EXIT_FAILURE);
    }

    backlog = atoi(ini_get_value(config, "inicializacion", "max_clients"));
    port = ini_get_value(config, "inicializacion", "listen_port");
    num_threads = atoi(ini_get_value(config, "inicializacion", "num_threads"));
    daemon = atoi(ini_get_value(config, "inicializacion", "daemon"));

    // Inicializacion del servidor
    if (server_init(daemon, num_threads, port, backlog)) {
        exit(EXIT_FAILURE);
    }

    fprintf(
      stdout, "Servidor: configurado y escuchando en el puerto %s...\n", port);

    // No necesitamos mas parametros de configuracion
    destroy_ini(config);
    cleanup_readini(ri);

    server_run();

    exit(EXIT_SUCCESS);
}
