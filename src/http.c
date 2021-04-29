/*****************************************************************************
 * ARCHIVO: http.c
 * DESCRIPCION: Implementacion de la interfaz de programacion para el
 * protocolo HTTP/1.1
 *
 * FECHA CREACION: 4 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/
#include <fcntl.h>        // open
#include <stdio.h>
#include <stdlib.h>       // NULL
#include <sys/socket.h>   // recv
#include <string.h>       // strcmp
#include <sys/sendfile.h> // sendfile
#include <sys/stat.h>     // stat
#include <sys/stat.h>     // open
#include <time.h>         // strftime
#include <unistd.h>       // chdir

#include "http.h"
#include "picohttpparser.h"

#define MAX_HTTP_REQUESTS_SIZE 4096 // Tamanyo maximo de la peticion
#define MAX_HTTP_NUM_HEADERS 100    // Numero maximo de cabeceras
#define MAX_HTTP_DATE_LEN 128 // Maxima longitud de la fecha
#define MAX_HTTP_HEADER 1024 // Tamanyo maximo de cabecera en la respuesta

typedef struct request_header {
    char* method;
    char* path;
    int version;
    size_t num_headers;
    struct phr_header* headers;
} request_header_t;

typedef struct request {
    request_header_t header;
    char* body;
} request_t;

char* get_response = "HTTP/1.%d 200 OK\r\nDate: %s\r\nServer: %s\r\nLast-Modified: %s\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n";
char* options_response = "HTTP/1.%d 200 OK\r\nDate: %s\r\nServer: %s\r\nContent-Length: 0\r\nAllow: GET, POST, OPTIONS\r\n\r\n";
char* error_400_response = "HTTP/1.1 400 Bad Request\r\nDate: %s\r\nConnection: close\r\nServer: %s\r\nContent-Length: 0\r\nContent-Type:text/html\r\n\r\n";
char* error_404_response = "HTTP/1.1 404 Not Found\r\nDate: %s\r\nConnection: close\r\nServer: %s\r\nContent-Length: 0\r\nContent-Type:text/html\r\n\r\n";

// Funciones privadas
static int http_parse_request(int socket, request_t* request);
static void http_free_request(request_t* request);
static int http_get(request_t request, int socket);
static int http_options(request_t request, int socket);
static void http_error_400_bad_request(int socket);
static void http_error_404_not_found(int socket);
// Funciones Auxiliares
static char* http_get_content_type(char* const file_extension);
static char* http_get_file_extension(char* filename);
static void http_get_date(char* date);
/* static void print_request(request_t request); */

void http(int socket)
{
    int status;
    request_t request;

    while (1) {
        memset(&request, 0, sizeof(request));
        status = http_parse_request(socket, &request);
        if (status == -1) {
            // Conexion cerrada por el cliente
            break;
        } else if (status == -2) {
            // Bad request
            http_error_400_bad_request(socket);
            break;
        } else if (status == -3) {
            printf("Request demasiado larga...\n");  
            break;
        }

        if (!strcmp("GET", request.header.method)) {
            status = http_get(request, socket);
            if (status == -1) {
                printf("El archivo no se ha encontrado...\n");
                http_error_404_not_found(socket);
                break;
            }  else if (status == -2) {
                printf("Error obteniendo la extension...\n");
                break;
            } else if (status == -3) {
                printf("Error obteniendo el tipo de contenido...\n");
                break;
            } else if (status == -4) {
                printf("Error enviando la informacion...\n");
                break;
            }
        } else if (!strcmp("OPTIONS", request.header.method)) {
            status = http_options(request, socket);
            if (status == -4) {
                printf("Error enviando la informacion...\n");
                break;
            }
        }

        http_free_request(&request);
    }

    http_free_request(&request);
}

static int http_parse_request(int socket, request_t* request)
{
    size_t i;
    char buf[MAX_HTTP_REQUESTS_SIZE];
    size_t offset = 0, prev_offset = 0, num_headers, method_len, path_len;
    ssize_t recv_ret;
    struct phr_header headers[MAX_HTTP_NUM_HEADERS];
    int pret, pret_total = 0, minor_version;
    const char* method = NULL;
    const char* path = NULL;

    memset(buf, 0, sizeof(buf));

    while (1) {
        recv_ret = recv(socket, buf + offset, sizeof(buf) - offset, 0);
        if (recv_ret <= 0) {
            // Cierre de conexion del cliente, error or timeout
            return -1;
        }

        prev_offset = offset;
        offset += recv_ret;

        num_headers = sizeof(headers) / sizeof(headers[0]);
        pret = phr_parse_request(buf,
                                 offset,
                                 &method,
                                 &method_len,
                                 &path,
                                 &path_len,
                                 &minor_version,
                                 headers,
                                 &num_headers,
                                 prev_offset);
        // Guardamos la longitud total de la cabecera
        pret_total += pret;
        if (pret > 0) {
            // Cabecera correctamente parseada
            break;
        } else if (pret == -1 || offset == sizeof(buf)) {
            // Error parseando la request
            return -2;
        } else if (offset == sizeof(buf)) {
            // Request demasiado larga
            return -3;
        }
    }

    // Almacenamos los datos de la request
    request->header.num_headers = num_headers;
    request->header.version = minor_version;

    request->header.method = (char*)malloc((method_len + 1) * sizeof(char));
    snprintf(request->header.method, method_len + 1, "%s", method);

    request->header.path = (char*)malloc((path_len + 1) * sizeof(char));
    snprintf(request->header.path, path_len + 1, "%s", path);

    request->header.headers =
      (struct phr_header*)malloc(num_headers * sizeof(struct phr_header));
    for (i = 0; i < request->header.num_headers; i++) {
        request->header.headers[i].name =
          (char*)malloc((headers[i].name_len + 1) * sizeof(char));
        snprintf((char*)request->header.headers[i].name,
                 headers[i].name_len + 1,
                 "%s",
                 headers[i].name);

        request->header.headers[i].value =
          (char*)malloc((headers[i].value_len + 1) * sizeof(char));
        snprintf((char*)request->header.headers[i].value,
                 headers[i].value_len + 1,
                 "%s",
                 headers[i].value);
    }

    // Almacenamos los datos del cuerpo de la request si existe
    // pret_total tiene la longitud de la cabecera de la request
    if (strlen(buf+pret_total) > 0) {
        request->body =
            (char*)malloc((strlen(buf+pret_total) + 1) * sizeof(char));
        sprintf(request->body, "%s", buf + pret_total);
    }

    return 0;
}

static void http_free_request(request_t* request)
{
    size_t i;

    if (request->header.method) {
        free(request->header.method);
    }
    if (request->header.path) {
        free(request->header.path);
    }
    for (i = 0; i < request->header.num_headers; i++) {
        if (request->header.headers[i].name) {
            free((char*)request->header.headers[i].name);
        }
        if (request->header.headers[i].value) {
            free((char*)request->header.headers[i].value);
        }
    }
    if (request->header.headers) {
        free(request->header.headers);
    }
    if (request->body) {
        free(request->body);
    }
}

static int http_get(request_t request, int socket)
{
    struct stat attr;
    int file_len;
    char last_modified[MAX_HTTP_DATE_LEN];
    char date[MAX_HTTP_DATE_LEN];
    char* content_type = NULL;
    char response_header[MAX_HTTP_HEADER];
    char server[7] = "perico\0";
    char path[100];
    int fd_file;
    ssize_t offset = 0;
    ssize_t bytes = 0;

    strcpy(path, "www");
    strcat(path, request.header.path);

    fd_file = open(path, O_RDONLY);

    if (fstat(fd_file, &attr) != 0) {
        return -1;
    }

    file_len = attr.st_size;

    // Ultima vez modificado
    strftime(last_modified,
             MAX_HTTP_DATE_LEN,
             "%a, %d %b %Y %H:%M:%S %Z",
             gmtime(&attr.st_mtime));

    // Tipo de fichero
    char* const file_extension = http_get_file_extension(path);
    if (!file_extension) {
        return -2;
    }
    content_type = http_get_content_type(file_extension);
    if (!content_type) {
        return -3;
    }

    // Obtengo la fecha para la cabecera date
    http_get_date(date);

    memset(response_header, 0,  MAX_HTTP_HEADER);

    sprintf(response_header,
            get_response,
            request.header.version,
            date,
            server,
            last_modified,
            file_len,
            content_type);

    do {
        bytes = send(socket, response_header + offset, strlen(response_header) - offset, 0);
        if (bytes == -1) {
            return -4;
        }
        offset += bytes;
    } while (offset < (ssize_t)strlen(response_header));
    offset = 0;
    do {
        bytes = sendfile(socket, fd_file, NULL, file_len);
        if (bytes == -1) {
            return -4;
        }
        offset += bytes;
    } while (offset < file_len);

    return 0;
}

static int http_options(request_t request, int socket)
{
    char date[MAX_HTTP_DATE_LEN], response_header[MAX_HTTP_HEADER];
    char server[7] = "perico\0";
    ssize_t bytes, offset=0;

    http_get_date(date);

    sprintf(response_header, options_response, request.header.version, date, server);

    do {
        bytes = send(socket, response_header + offset, strlen(response_header) - offset, 0);
        if (bytes == -1) {
            return -4;
        }
        offset += bytes;
    } while (offset < (ssize_t)strlen(response_header));

    return 0;
}

static char* http_get_content_type(char* const file_extension)
{
    if (!strcmp("txt", file_extension))
        return "text/plain";
    if (!strcmp("htm", file_extension) || !strcmp("html", file_extension))
        return "text/html";
    if (!strcmp("gif", file_extension))
        return "image/gif";
    if (!strcmp("jpg", file_extension) || !strcmp("jpeg", file_extension) || !strcmp("ico", file_extension))
        return "image/jpeg";
    if (!strcmp("mpg", file_extension) || !strcmp("mpeg", file_extension) || !strcmp("mkv", file_extension))
        return "Content-Type: video/mpeg";
    if (!strcmp("doc", file_extension) || !strcmp("docx", file_extension))
        return "application/msword";
    if (!strcmp("pdf", file_extension))
        return "application/pdf";

    return NULL;
}

static char* http_get_file_extension(char* filename)
{
    char* dot = strrchr(filename, '.');
    if (!dot) {
        return NULL;
    }
    return dot + 1;
}

static void http_get_date(char* date)
{
    time_t now = time(0);
    struct tm *tm = gmtime(&now);

    strftime(date, MAX_HTTP_DATE_LEN, "%a, %d %b %Y %H:%M:%S %Z", tm);
}

static void http_error_400_bad_request(int socket)
{
    char date[MAX_HTTP_DATE_LEN], response_header[MAX_HTTP_HEADER];
    char server[7] = "perico\0";
    sprintf(response_header, error_400_response, date, server);
    send(socket, response_header, strlen(response_header), 0);
}

static void http_error_404_not_found(int socket)
{
    char date[MAX_HTTP_DATE_LEN], response_header[MAX_HTTP_HEADER];
    char server[7] = "perico\0";
    sprintf(response_header, error_404_response, date, server);
    send(socket, response_header, strlen(response_header), 0);
}

/* static void print_request(request_t request) */
/* { */
/*     size_t i; */

/*     printf("****************************************************\n"); */
/*     printf("Method is: %s\n", request.header.method); */
/*     printf("Path is: %s\n", request.header.path); */
/*     printf("Version is: 1.%d\n", request.header.version); */
/*     for (i = 0; i < request.header.num_headers; i++) { */
/*         printf("%s : %s\n", */
/*                request.header.headers[i].name, */
/*                request.header.headers[i].value); */
/*     } */
/*     printf("Body is:\n %s", request.body); */
/*     printf("****************************************************\n"); */
/* } */
