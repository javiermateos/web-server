/*****************************************************************************
 * ARCHIVO: http.c
 * DESCRIPCION: Implementacion de la interfaz de programacion para el
 * protocolo HTTP/1.1
 *
 * FECHA CREACION: 4 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/
#include <assert.h> // assert
#include <stdio.h>
#include <stdlib.h>     // malloc
#include <sys/socket.h> // recv
#include <unistd.h>     // read
#include <time.h> // localtime
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "http.h"
#include "picohttpparser.h"

#define MAX_HTTP_HEADER 4096     // Tamanyo maximo de la cabecera
#define MAX_HTTP_NUM_HEADERS 100 // Numero maximo de headers
#define MAX_DATE_LEN 128

typedef struct request_header {
    char* method;
    char* path;
    int version;
    size_t num_headers;
    struct phr_header* headers;
} request_header_t;

typedef struct request {
    request_header_t* header;
    char* body;
} request_t;

char* get_response = "HTTP/1.%d 200 OK\r\nDate: %s\r\nServer: %s\r\nLast-Modified: %s\r\nContent-Length: %d\r\nContent-Type: %s\r\n";


// Private Functions
request_header_t* http_parse_header(int socket);
void http_free_request(request_t request);
void http_get(request_t request, int socket);
char* get_filename_ext(char *filename);
char* http_get_content_type(char* const file_extension);
void http_400_bad_request(request_t request);

void http(int socket)
{
    request_t request;

    do {
        request.header = http_parse_header(socket);

        // Comprobar condicion de socket cerrado
                
        if (!strcmp("POST",request.header->method)) {

        } else if (!strcmp("GET",request.header->method)) {
            http_get(request, socket);
        } else if (!strcmp("OPTIONS",request.header->method)) {

        }
        // write repose

        http_free_request(request);
        return;
    } while (1);
}

request_header_t* http_parse_header(int socket)
{
    int pret;
    size_t i;
    size_t method_len, path_len, num_headers;
    size_t header_message_offset = 0, prev_header_message_offset = 0;
    ssize_t recv_ret;
    char header_message[MAX_HTTP_HEADER];
    const char *method, *path;
    struct phr_header headers[100];
    request_header_t* header = NULL;

    header = (request_header_t*)malloc(sizeof(request_header_t));
    if (!header) {
        return NULL;
    }

    while (1) {
        recv_ret = recv(socket,
                        header_message + header_message_offset,
                        sizeof(header_message) + header_message_offset,
                        0);
        if (recv_ret <= 0) {
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
        } else if (pret == -1 &&
                   header_message_offset == sizeof(header_message)) {
            return NULL; // error
        }
    }

    header->num_headers = num_headers;

    header->method = (char*)malloc((method_len + 1) * sizeof(char));
    snprintf(header->method, method_len + 1, "%s", method);

    header->path = (char*)malloc((path_len + 1) * sizeof(char));
    snprintf(header->path, path_len + 1, "%s", path);

    header->headers =
      (struct phr_header*)malloc(num_headers * sizeof(struct phr_header));
    for (i = 0; i < header->num_headers; i++) {
        header->headers[i].name =
          (char*)malloc((headers[i].name_len + 1) * sizeof(char));
        snprintf((char*)header->headers[i].name,
                 headers[i].name_len + 1,
                 "%s",
                 headers[i].name);

        header->headers[i].value =
          (char*)malloc((headers[i].value_len + 1) * sizeof(char));
        snprintf((char*)header->headers[i].value,
                 headers[i].value_len + 1,
                 "%s",
                 headers[i].value);
    }

    return header;
}

void http_free_request(request_t request)
{
    size_t i;

    // Header
    free(request.header->method);
    free(request.header->path);
    for (i = 0; i < request.header->num_headers; i++) {
        free((char*)request.header->headers[i].name);
        free((char*)request.header->headers[i].value);
    }
    free(request.header->headers);
    free(request.header);
}


void http_get(request_t request, int socket)
{
    char date[100];
    time_t now = time(0);
    struct tm tm = *gmtime(&now);
    char path[100];
    char response_header[MAX_HTTP_HEADER];
    char server[7] = "perico\0";
    FILE *file;
    struct stat attr;
    char last_modified[MAX_DATE_LEN];
    int file_len = 0;
    char *content_type;
    
    // Obtengo la fecha para la cabecera date
    strftime(date, sizeof date, "%a, %d %b %Y %H:%M:%S %Z", &tm);

    // Asigno el directorio a la ruta donde se encuentran los contenidos del servidor
    chdir("./www/");

    // Elimino del path la primera '/'
    strcpy(path, request.header->path);
    memmove(path, path+1, strlen(path));

    // Ultima vez modificado
    stat(path, &attr);
    strftime(last_modified, MAX_DATE_LEN, "%a, %d %b %Y %H:%M:%S %Z", gmtime(&attr.st_mtime));

    // Tipo de fichero
    char* const file_extension = get_filename_ext(path);
    content_type = http_get_content_type(file_extension);

    
    file = fopen(path,"rb");

    // Obtener longitud del fichero y reseteo el cursor
    fseek(file, 0, SEEK_END);
    file_len = ftell(file);
    fseek(file, 0, SEEK_SET);

    sprintf(response_header, get_response, request.header->version, date, server, last_modified, file_len, content_type);
    write(socket, response_header,strlen(response_header));

    fclose(file);
}

 char * get_filename_ext(char *filename) {
    char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

char* http_get_content_type(char* const file_extension)
{
    if (!strcmp("txt", file_extension))  return "text/plain";
    if (!strcmp("htm", file_extension) || !strcmp("html", file_extension))  return "text/html";
    if (!strcmp("gif", file_extension))  return "image/gif";
    if (!strcmp("jpg", file_extension) || !strcmp("jpeg", file_extension))  return "image/jpeg";
    if (!strcmp("mpg", file_extension) || !strcmp("mpeg", file_extension))  return "Content-Type: video/mpeg\r\n\r\n";
    if (!strcmp("doc", file_extension) || !strcmp("docx", file_extension))  return "application/msword";
    if (!strcmp("pdf", file_extension))  return "application/pdf";

    return "NonType";
}


