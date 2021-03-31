/*****************************************************************************
 * ARCHIVO: http.c
 * DESCRIPCION: Implementacion de la interfaz de programacion para el
 * protocolo HTTP/1.1
 *
 * FECHA CREACION: 4 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/
#include <assert.h> //assert
#include <stdio.h>
#include <stdlib.h> //malloc
#include <unistd.h> //read
#include <sys/socket.h> //recv

#include "http.h"
#include "picohttpparser.h"

#define MAX_HTTP_HEADER 4096 // Tamanyo maximo de la cabecera
#define MAX_HTTP_NUM_HEADERS 100 // Numero maximo de headers


typedef struct request_header {
    char* method;
    char* path;
    int version;
    size_t num_headers;
    struct phr_header *headers;
} request_header_t;

typedef struct request {
    request_header_t* header;
    char* body;
} request_t;


// Private Functions
request_header_t* http_parse_header(int socket);

void http(int socket)
{
    request_t request;
    ssize_t readLen;
    size_t i;

    
    do{
        
        request.header = http_parse_header(socket);

        // Comprobar condicion de socket cerrado
        /**
        if (strcmp("POST",request.header->method)) {

        } else if (strcmp("GET",request.header->method)) {

        } else if (strcmp("OPTIONS",request.header->method)) {

        }
        */
        // write repose

        free(request.header->method);
        free(request.header->path);
/**
        for (i = 0; i < request.header->num_headers; i++) {
            free(request.header->headers[i].name);
            free(request.header->headers[i].value);
        } 
*/
        // free(request.header->headers);
        free(request.header);

        return;

    } while (1);


}

request_header_t* http_parse_header(int socket)
{
    request_header_t* header = NULL;
    char header_message[MAX_HTTP_HEADER];
    size_t header_message_offset = 0, prev_header_message_offset = 0;
    ssize_t recv_ret;
    int pret;

    // Variables auxiliares para el parseo
    size_t method_len, path_len, num_headers;
    const char *method, *path;
    struct phr_header headers[100];
    size_t i;


    header = (request_header_t*)malloc(sizeof(request_header_t));
    if (!header) {
        return NULL;
    }


    while (1) {

        recv_ret = recv(socket, header_message + header_message_offset, sizeof(header_message) + header_message_offset, 0);
        if (recv_ret <= 0){
            return NULL;
        }
        
        prev_header_message_offset = header_message_offset;
        header_message_offset += recv_ret;
       

        num_headers = sizeof(headers) / sizeof(headers[0]);
        pret = phr_parse_request(header_message,
                                 header_message_offset,
                                 &method,
                                 &method_len,
                                 &path,
                                 &path_len,
                                 &header->version,
                                 headers,
                                 &num_headers,
                                 prev_header_message_offset);

        if (pret > 0) {
            break; // cabecera correctamente parseada
        } else if (pret == -1 && header_message_offset == sizeof(header_message)) {
            return NULL; // error
        } 
    }

    header->num_headers =  num_headers;

    header->method = (char*)malloc(sizeof(method_len + 1));
    snprintf(header->method, method_len + 1, "%s", method);

    header->path = (char*)malloc(sizeof(path_len + 1));
    snprintf(header->path, path_len + 1, "%s", path);


    header->headers = (struct phr_header*)malloc(num_headers * sizeof(struct phr_header));
    for (i = 0; i < header->num_headers; i++) {
        header->headers[i].name = (char*)malloc(sizeof(headers[i].name_len + 1));
        snprintf((char*)header->headers[i].name,
                 headers[i].name_len + 1,
                 "%s",
                 headers[i].name);

        header->headers[i].value = (char*)malloc(sizeof(headers[i].value_len + 1));

        snprintf((char*)header->headers[i].value,
                 headers[i].value_len + 1,
                 "%s",
                 headers[i].value);

    }

    printf("method is %s\n",header->method);
    printf("path is %s\n", header->path);
    printf("HTTP version is 1.%d\n", header->version);

    printf("headers:\n");
    for (i = 0; i < header->num_headers; i++) {
        // printf("nl: %ld\n", headers[i].name_len);
        // printf("vl: %ld\n", headers[i].value_len);
        printf("%s: %s\n", header->headers[i].name, header->headers[i].value);
    }    

   // printf("ml: %ld\n", method_len);
   // printf("pl: %ld\n", path_len);
   return header;
}
