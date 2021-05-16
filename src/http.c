/******************************************************************************
 * ARCHIVO: http.c
 * DESCRIPCION: Implementacion de la interfaz de programacion para el
 * protocolo HTTP/1.1
 *
 * FECHA CREACION: 4 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 ******************************************************************************/
#include <fcntl.h>        // open
#include <stdlib.h>       // NULL
#include <string.h>       // strcmp
#include <sys/sendfile.h> // sendfile
#include <sys/socket.h>   // recv
#include <sys/stat.h>     // stat
#include <sys/stat.h>     // open
#include <sys/wait.h>     // wait
#include <time.h>         // strftime

#include "http.h"
#include "picohttpparser.h"
#include "socket.h"

#define MAX_HTTP_REQUESTS_SIZE 4096 // Tamanyo maximo de la peticion
#define MAX_HTTP_NUM_HEADERS 100    // Numero maximo de cabeceras
#define MAX_HTTP_DATE_LEN 128       // Maxima longitud de la fecha
#define MAX_HTTP_HEADER 1024       // Tamanyo maximo de cabecera en la respuesta
#define MAX_HTTP_ERRORS 5          // Numero de errores del servidor
#define MAX_HTTP_PATH 100          // Tamanyo maximo del path
#define MAX_HTTP_COMMAND 200       // Tamanyo maximo del comando CGI
#define MAX_HTTP_CGI_RESPONSE 3072 // Tamanyo maximo de la respuesta CGI

// Definicion de los errores del protocolo http
typedef enum error {
    BAD_REQUEST,            // BAD_REQUEST
    NOT_FOUND,              // NOT_FOUND
    NOT_IMPLEMENTED,        // NOT_IMPLEMENTED
    UNSUPPORTED_MEDIA_TYPE, // UNSUPPORTED_MEDIA_TYPE
    INTERNAL_SERVER_ERROR,  // INTERNAL_SERVER_ERROR
    OK,                     // OK
} error_t;

// Definicion de la cabecera del protocolo http
typedef struct request_header {
    char* method;               // Metodo del protocolo
    char* path;                 // Path del recurso
    int version;                // Version del protocolo http
    size_t num_headers;         // Numero de cabeceras de la peticion
    struct phr_header* headers; // Estructura con las cabeceras de la peticion
} request_header_t;

// Request http
typedef struct request {
    request_header_t header; // Cabecera de la request
    char* body;              // Cuerpo de la request
} request_t;

// Cadena con la respuesta a una peticion GET
char* get_response =
  "HTTP/1.%d 200 OK\r\nDate: %s\r\nServer: %s\r\nLast-Modified: "
  "%s\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n";

// Cadena con la respuesta a una peticion POST
char* post_response =
  "HTTP/1.%d 200 OK\r\nDate: %s\r\nServer: %s\r\nLast-Modified: "
  "%s\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n";

// Cadena con la respuesta a una peticion OPTIONS
char* options_response =
  "HTTP/1.%d 200 OK\r\nDate: %s\r\nConnection: close\r\nServer: "
  "%s\r\nContent-Length: 0\r\nAllow: GET, POST, OPTIONS\r\n\r\n";

// Cadenas generadas para enviar la respuesta si se produce un error
char* error_response[MAX_HTTP_ERRORS] = {
    "HTTP/1.1 400 Bad Request\r\nDate: %s\r\nConnection: close\r\nServer: "
    "%s\r\nContent-Length: 0\r\nContent-Type:text/html\r\n\r\n",
    "HTTP/1.1 404 Not Found\r\nDate: %s\r\nConnection: close\r\nServer: "
    "%s\r\nContent-Length: 0\r\nContent-Type:text/html\r\n\r\n",
    "HTTP/1.1 501 Not Implemented\r\nDate: %s\r\nConnection: close\r\nServer: "
    "%s\r\nContent-Length: 0\r\nContent-Type:text/html\r\n\r\n",
    "HTTP/1.1 415 Unsupported Media Type\r\nDate: %s\r\nConnection: "
    "close\r\nServer: "
    "%s\r\nContent-Length: 0\r\nContent-Type:text/html\r\n\r\n",
    "HTTP/1.1 500 Internal Server Error\r\nDate: %s\r\nConnection: "
    "close\r\nServer: %s\r\nContent-Length: "
    "0\r\nContent-Type:text/html\r\n\r\n",
};

// Funciones privadas

/******************************************************************************
 * FUNCION: static int http_parse_request(int socket, request_t* request)
 * ARGS_IN: int socket - socket donde se van a recibir las peticiones.
 *          request_t* request - estructura donde se va a almacenar la request
 *                               recibida.
 * DESCRIPCION: almacena lapeticion leida en la estructura recibida como
 *              argumento.
 * ARGS_OUT: int - codigo de la estructura error.
 *****************************************************************************/
static int http_parse_request(int socket, request_t* request);

/*****************************************************************************
 * FUNCION: static void http_free_request(request_t* request)
 * ARGS_IN: request_t* request - peticion a liberar.
 * DESCRIPCION: libera la memoria de una peticion.
 *****************************************************************************/
static void http_free_request(request_t* request);

/******************************************************************************
 * FUNCION: http_get(request_t request,
 *                  int socket,
 *                  char* server_root,
 *                  char* server_signature)
 * ARGS_IN: request_t request - peticion a procesar.
 *          int socket - socket donde esta establecida la conexion.
 *          char* server_root - ruta donde estan los recursos del servidor.
 *          char* server_signature - nombre del servidor.
 * DESCRIPCION: procesa y genera la respuesta a las peticiones de metodo GET
 *              recibidas por el servidor.
 * ARGS_OUT: int - codigo de la estuctura error.
 *****************************************************************************/
static int http_get(request_t request,
                    int socket,
                    char* server_root,
                    char* server_signature);

/******************************************************************************
 * FUNCION: http_post(request_t request,
 *                   int socket,
 *                   char* server_root,
 *                   char* server_signature)
 * ARGS_IN: request_t request - peticion a procesar.
 *          int socket - socket donde esta establecida la conexion.
 *          char* server_root - ruta donde estan los recursos del servidor.
 *          char* server_signature - nombre del servidor.
 * DESCRIPCION: procesa y genera la respuesta a las peticiones de metodo POST
 *              recibidas por el servidor.
 * ARGS_OUT: int - codigo de la estuctura error.
 *****************************************************************************/
static int http_post(request_t request,
                     int socket,
                     char* server_root,
                     char* server_signature);

/******************************************************************************
 * FUNCION: static int http_options(request_t request,
 *                                 int socket,
 *                                 char* server_signature)
 * ARGS_IN: request_t request - peticion a procesar.
 *          int socket - socket donde esta establecida la conexion.
 *          char* server_signature - nombre del servidor.
 * DESCRIPCION: procesa y genera la respuesta a las peticiones de metodo
 *              OPTIONS recibidas por el servidor.
 * ARGS_OUT: int - codigo de la estuctura error.
 *****************************************************************************/
static int http_options(request_t request, int socket, char* server_signature);

/******************************************************************************
 * FUNCION: static void http_error(int socket,
 *                                char* server_signature,
 *                                error_t error)
 * ARGS_IN: int socket - socket donde esta establecida la conexion.
 *          char* server_signature - nombre del servidor.
 *          error_t error - tipo de error obtenido.
 * DESCRIPCION: envia el error obtenido como respuesta a la peticion recibida.
 *****************************************************************************/
static void http_error(int socket, char* server_signature, error_t error);

// Funciones Auxiliares

/******************************************************************************
 * FUNCION: static char* http_get_content_type(const char* file_extension)
 * ARGS_IN: const char* file_extension - ruta al fichero.
 * DESCRIPCION: obtiene la extension del fichero que se va a procesar.
 * ARGS_OUT: char* - extension del fichero.
 *****************************************************************************/
static char* http_get_content_type(const char* file_extension);

/******************************************************************************
 * FUNCION: static void http_get_date(char* date)
 * ARGS_IN: char* date - cadena donde se guarda la fecha con el formato GMT.
 * DESCRIPCION: obtiene la hora en formato GMT en la cadena recibida como
 *              argumento.
 *****************************************************************************/
static void http_get_date(char* date);

int http(int socket, char* server_root, char* server_signature)
{
    int status;
    request_t request;

    while (1) {
        memset(&request, 0, sizeof(request));
        status = http_parse_request(socket, &request);
        if (status == -1) {
            // Conexion cerrada por el cliente
            break;
        } else if (status == BAD_REQUEST) {
            // Bad request
            http_error(socket, server_signature, BAD_REQUEST);
            break;
        }

        if (!strcmp("GET", request.header.method)) {
            status = http_get(request, socket, server_root, server_signature);
            if (status == BAD_REQUEST) {
                http_error(socket, server_signature, BAD_REQUEST);
                break;
            } else if (status == NOT_FOUND) {
                http_error(socket, server_signature, NOT_FOUND);
                break;
            } else if (status == INTERNAL_SERVER_ERROR) {
                http_error(socket, server_signature, INTERNAL_SERVER_ERROR);
                break;
            }
        } else if (!strcmp("POST", request.header.method)) {
            status = http_post(request, socket, server_root, server_signature);
            if (status == BAD_REQUEST) {
                http_error(socket, server_signature, BAD_REQUEST);
                break;
            } else if (status == NOT_FOUND) {
                http_error(socket, server_signature, NOT_FOUND);
                break;
            } else if (status == INTERNAL_SERVER_ERROR) {
                http_error(socket, server_signature, INTERNAL_SERVER_ERROR);
                break;
            } else if (status == UNSUPPORTED_MEDIA_TYPE) {
                http_error(socket, server_signature, UNSUPPORTED_MEDIA_TYPE);
                break;
            }
        } else if (!strcmp("OPTIONS", request.header.method)) {
            status = http_options(request, socket, server_signature);
            if (status == INTERNAL_SERVER_ERROR) {
                http_error(socket, server_signature, INTERNAL_SERVER_ERROR);
                break;
            }
        } else {
            http_error(socket, server_signature, NOT_IMPLEMENTED);
            break;
        }

        if (status == INTERNAL_SERVER_ERROR) {
            return -1;
        }

        http_free_request(&request);
    }

    http_free_request(&request);

    return 0;
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
        } else if (pret == -1) {
            // Error parseando la request
            return BAD_REQUEST;
        } else if (offset == sizeof(buf)) {
            // Request demasiado larga
            return BAD_REQUEST;
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
    if (strlen(buf + pret_total) > 0) {
        request->body =
          (char*)malloc((strlen(buf + pret_total) + 1) * sizeof(char));
        sprintf(request->body, "%s", buf + pret_total);
    }

    return OK;
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

static int http_get(request_t request,
                    int socket,
                    char* server_root,
                    char* server_signature)
{
    int response_body_len;
    struct stat attr;
    char last_modified[MAX_HTTP_DATE_LEN];
    char date[MAX_HTTP_DATE_LEN];
    char response_header[MAX_HTTP_HEADER];
    char path[MAX_HTTP_PATH];
    char command[MAX_HTTP_COMMAND];
    char* response_body;
    char* content_type = NULL;
    char* args = NULL;
    FILE* file = NULL;

    // Parseamos los argumentos si existen
    if (strstr(request.header.path, "?")) {
        args = strrchr(request.header.path, '?');
        *args = '\0';
        args++;
    }

    // Obtenemos el path del recurso
    strcpy(path, server_root);
    strcat(path, request.header.path);

    if (args) {
        if (strstr(path, ".py")) {
            sprintf(command, "python3 %s %s 2>&1", path, args);
        } else if (strstr(path, ".php")) {
            sprintf(command, "php %s %s 2>&1", path, args);
        } else {
            return BAD_REQUEST;
        }

        file = popen(command, "r");
        if (!file) {
            return NOT_FOUND;
        }
        // Nota: Puesto que este "file" no es un stream normal, no podemos
        // determinar el tamanio que tendrá, ya que en realidad se trata de
        // de un pipe.
        response_body_len = MAX_HTTP_CGI_RESPONSE;
    } else {
        file = fopen(path, "rb");
        if (!file) {
            return NOT_FOUND;
        }
        fseek(file, 0L, SEEK_END);
        response_body_len = ftell(file);
        fseek(file, 0L, SEEK_SET);
    }

    response_body = (char*)malloc((response_body_len + 1) * sizeof(char));
    memset(response_body, 0, response_body_len);
    fread(response_body, 1, response_body_len, file);

    // Ultima vez modificado
    stat(path, &attr);
    strftime(last_modified,
             MAX_HTTP_DATE_LEN,
             "%a, %d %b %Y %H:%M:%S %Z",
             gmtime(&attr.st_mtime));

    // Tipo de fichero
    content_type = http_get_content_type(path);
    if (!content_type) {
        return UNSUPPORTED_MEDIA_TYPE;
    }

    // Obtengo la fecha para la cabecera date
    http_get_date(date);

    memset(response_header, 0, MAX_HTTP_HEADER);
    sprintf(response_header,
            get_response,
            request.header.version,
            date,
            server_signature,
            last_modified,
            response_body_len,
            content_type);

    if (socket_send(
          socket, response_header, response_body, response_body_len) == -1) {
        return INTERNAL_SERVER_ERROR;
    }

    if (args) {
        pclose(file);
    } else {
        fclose(file);
    }

    free(response_body);

    return 0;
}

static int http_options(request_t request, int socket, char* server_signature)
{
    char date[MAX_HTTP_DATE_LEN], response_header[MAX_HTTP_HEADER];
    ssize_t bytes, offset = 0;

    http_get_date(date);

    sprintf(response_header,
            options_response,
            request.header.version,
            date,
            server_signature);

    do {
        bytes = send(socket,
                     response_header + offset,
                     strlen(response_header) - offset,
                     0);
        if (bytes == -1) {
            return -4;
        }
        offset += bytes;
    } while (offset < (ssize_t)strlen(response_header));

    return 0;
}

static int http_post(request_t request,
                     int socket,
                     char* server_root,
                     char* server_signature)
{
    int response_body_len;
    struct stat attr;
    char last_modified[MAX_HTTP_DATE_LEN];
    char date[MAX_HTTP_DATE_LEN];
    char response_header[MAX_HTTP_HEADER];
    char path[MAX_HTTP_PATH];
    char command[MAX_HTTP_COMMAND];
    char* response_body;
    char* content_type = NULL;
    char* args = NULL;
    FILE* file = NULL;

    // Eliminamos los argumentos del path si existen
    if (strstr(request.header.path, "?")) {
        args = strrchr(request.header.path, '?');
        *args = '\0';
        args++;
    }

    // Obtenemos el path del recurso
    strcpy(path, server_root);
    strcat(path, request.header.path);

    if (request.body) {
        if (strstr(path, ".py")) {
            sprintf(command, "python3 %s %s 2>&1", path, request.body);
        } else if (strstr(path, ".php")) {
            sprintf(command, "php %s %s 2>&1", path, request.body);
        } else {
            return BAD_REQUEST;
        }
    } else {
        if (strstr(path, ".py")) {
            sprintf(command, "python3 %s 2>&1", path);
        } else if (strstr(path, ".php")) {
            sprintf(command, "php %s 2>&1", path);
        } else {
            return BAD_REQUEST;
        }
    }

    file = popen(command, "r");
    if (!file) {
        return NOT_FOUND;
    }

    response_body_len = MAX_HTTP_CGI_RESPONSE;
    response_body = (char*)malloc((response_body_len + 1) * sizeof(char));
    memset(response_body, 0, response_body_len);
    fread(response_body, 1, response_body_len, file);

    // Ultima vez modificado
    stat(path, &attr);
    strftime(last_modified,
             MAX_HTTP_DATE_LEN,
             "%a, %d %b %Y %H:%M:%S %Z",
             gmtime(&attr.st_mtime));

    // Tipo de fichero
    content_type = http_get_content_type(path);
    if (!content_type) {
        return UNSUPPORTED_MEDIA_TYPE;
    }

    // Obtengo la fecha para la cabecera date
    http_get_date(date);

    memset(response_header, 0, MAX_HTTP_HEADER);
    sprintf(response_header,
            get_response,
            request.header.version,
            date,
            server_signature,
            last_modified,
            response_body_len,
            content_type);

    if (socket_send(
          socket, response_header, response_body, response_body_len) == -1) {
        return INTERNAL_SERVER_ERROR;
    }

    if (args) {
        pclose(file);
    } else {
        fclose(file);
    }

    free(response_body);

    return 0;
}

static char* http_get_content_type(const char* path)
{
    const char* file_extension = NULL;

    file_extension = strrchr(path, '.') + 1;
    if (!file_extension) {
        return NULL;
    }

    if (!strcmp("txt", file_extension))
        return "text/plain";
    if (!strcmp("htm", file_extension) || !strcmp("html", file_extension) ||
        !strcmp("py", file_extension) || !strcmp("php", file_extension))
        return "text/html";
    if (!strcmp("gif", file_extension))
        return "image/gif";
    if (!strcmp("jpg", file_extension) || !strcmp("jpeg", file_extension) ||
        !strcmp("ico", file_extension))
        return "image/jpeg";
    if (!strcmp("mpg", file_extension) || !strcmp("mpeg", file_extension) ||
        !strcmp("mkv", file_extension))
        return "Content-Type: video/mpeg";
    if (!strcmp("doc", file_extension) || !strcmp("docx", file_extension))
        return "application/msword";
    if (!strcmp("pdf", file_extension))
        return "application/pdf";

    return NULL;
}

static void http_get_date(char* date)
{
    time_t now = time(0);
    struct tm* tm = gmtime(&now);

    strftime(date, MAX_HTTP_DATE_LEN, "%a, %d %b %Y %H:%M:%S %Z", tm);
}

static void http_error(int socket, char* server_signature, error_t error)
{
    char date[MAX_HTTP_DATE_LEN], response_header[MAX_HTTP_HEADER];
    http_get_date(date);
    sprintf(response_header, error_response[error], date, server_signature);
    send(socket, response_header, strlen(response_header), 0);
}
