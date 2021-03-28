/*****************************************************************************
 * ARCHIVO: http.c
 * DESCRIPCION: Implementacion de la interfaz de programacion para el 
 * protocolo HTTP/1.1
 *
 * FECHA CREACION: 4 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h> //malloc
#include <unistd.h> //read
#include <assert.h> //assert


#include "picohttpparser.h"
#include "http.h"

struct http_request
{
    char *method;
    char *path;
    int version; 
    int size;
    int num_headers;
    struct phr_header *headers;
}http_request;


int http(int socket)
{
    char buf[4096];
    const char *method, *path;
    int pret, minor_version, i;
    struct phr_header headers[100];
    size_t buflen = 0, prevBufLen = 0, num_headers, path_len, method_len;
    ssize_t readRet;

    struct http_request parsed_request;


    while(1){
        while ((readRet = read(socket, buf + buflen, sizeof(buf) + buflen)) == -1 );

        if (readRet <= 0) {
            return -1;
        }
        prevBufLen = buflen;
        buflen += readRet;
        
        num_headers = sizeof(headers) / sizeof(headers[0]);
        pret = phr_parse_request(buf, buflen, &method, &method_len, &path, &path_len, &minor_version, headers, &num_headers, prevBufLen);

        if (pret > 0){
            break; /* successfully parsed the request */
        }
        else if (pret == -1){
            return -1;
        }

        // request too long
        if (buflen == sizeof(buf)){
            return -2;
        }
    }

    parsed_request.size = pret;
    parsed_request.version = minor_version;
    parsed_request.num_headers = num_headers;

    parsed_request.method= (char*)malloc(sizeof((int)method_len));
    snprintf(parsed_request.method, pret, "%.*s", (int)method_len, method);

    parsed_request.path= (char*)malloc(sizeof((int)path_len));
    snprintf(parsed_request.path, pret, "%.*s", (int)path_len, path);


    parsed_request.path= (char*)malloc(sizeof((int)path_len));
    snprintf(parsed_request.path, pret, "%.*s", (int)path_len, path);

    parsed_request.headers = (struct phr_header*)malloc(num_headers*sizeof(struct phr_header)); 
    for(i=0;i<parsed_request.num_headers;i++){
        parsed_request.headers[i].name = (char*)malloc(sizeof((int)headers[i].name_len));
        snprintf((char*)parsed_request.headers[i].name, pret, "%.*s", (int)headers[i].name_len, headers[i].name);

        parsed_request.headers[i].value= (char*)malloc(sizeof((int)headers[i].value_len));
        snprintf((char*)parsed_request.headers[i].value, pret, "%.*s", (int)headers[i].value_len, headers[i].value);
    }

    printf("request is %d bytes long\n", parsed_request.size);
    printf("method is %s\n", parsed_request.method);
    printf("path is %s\n", parsed_request.path);
    printf("HTTP version is 1.%d\n", parsed_request.version);
    printf("Headers:\n");
    for (i = 0; i != parsed_request.num_headers; ++i) {
        printf("%s: %s\n", parsed_request.headers[i].name, parsed_request.headers[i].value);
    }

    return 0;
}