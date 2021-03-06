/*****************************************************************************
 * ARCHIVO: tpool.c
 * DESCRIPCION: Implementacion de la interfaz del pool de threads
 *
 * NOTA: Las condiciones de espera en las funciones estan rodeadas por bucles
 * que comprueban si las condiciones de espera verdaderamente deben interrumpirse.
 *
 * https://www.man7.org/linux/man-pages/man3/pthread_cond_broadcast.3p.html#top_of_page
 *
 * FECHA CREACION: 4 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/

#include <pthread.h>
#include <stdlib.h>

#include "tpool.h"

// Contenedor de trabajo
typedef struct tpool_work {
    thread_func_t func;      // Funcion trabajo
    void* arg;               // Argumentos de la funcion
    struct tpool_work* next; // Siguiente trabajo de la cola de trabajos
} tpool_work_t;

// Contenedor de hilos
struct tpool {
    tpool_work_t* work_first;    // Primer elemento de la cola de trabajos
    tpool_work_t* work_last;     // Ultimo elemento de la cola de trabajos
    pthread_mutex_t work_mutex;  // Sincroniza el acceso a la cola de trabajo
    pthread_cond_t work_cond;    // Indica que hay trabajo que procesar
    pthread_cond_t working_cond; // Indica que no hay threads procesando
    size_t working_cnt;          // Indica cuantos threads hay trabajando
    size_t thread_cnt;           // Indica cuantos threads estan vivos
    bool stop;                   // Para los hilos
};

// Funciones privadas
static tpool_work_t* tpool_work_create(thread_func_t func, void* arg);
static void tpool_work_destroy(tpool_work_t* work);
static tpool_work_t* tpool_get_work(tpool_t* tm);
static void* tpool_worker(void* arg);

static tpool_work_t* tpool_work_create(thread_func_t func, void* arg)
{
    tpool_work_t* work = NULL;

    if (!func) {
        return NULL;
    }

    work = (tpool_work_t*)malloc(sizeof(tpool_work_t));
    work->func = func;
    work->arg = arg;
    work->next = NULL;

    return work;
}

static void tpool_work_destroy(tpool_work_t* work)
{
    if (!work) {
        return;
    }

    free(work);
}

// Extrae de la cola de trabajos el siguiente trabajo a procesar por los hilos
static tpool_work_t* tpool_get_work(tpool_t* tm)
{
    tpool_work_t* work = NULL;

    if (!tm) {
        return NULL;
    }

    work = tm->work_first;
    if (!work) {
        return NULL;
    }

    // Reorganizamos la cola de trabajos del pool de hilos
    if (!work->next) {
        // Ultimo trabajo
        tm->work_first = NULL;
        tm->work_last = NULL;
    } else {
        tm->work_first = work->next;
    }

    return work;
}

// Se encarga de esperar a que haya trabajos y los procesa.
static void* tpool_worker(void* arg)
{
    tpool_t* tm = arg;
    tpool_work_t* work = NULL;

    // Mantenemos el hilo corriendo hasta que deba parar
    while (1) {
        pthread_mutex_lock(&(tm->work_mutex));
        // Espera hasta que haya trabajos o se deba parar los hilos
        while (!tm->work_first && !tm->stop) {
            // Espramos hasta que llegue la senial de que hay trabajo
            pthread_cond_wait(&(tm->work_cond), &(tm->work_mutex));
        }
        // Comprobamos si hay que parar el hilo
        if (tm->stop) {
            break;
        }
        // Extraemos un trabajo de la cola de trabajos
        work = tpool_get_work(tm);
        tm->working_cnt++;
        pthread_mutex_unlock(&(tm->work_mutex));

        if (work) {
            // Manda el trabajo a un hilo
            work->func(work->arg);
            tpool_work_destroy(work);
        }

        // El trabajo ha sido procesado
        pthread_mutex_lock(&(tm->work_mutex));
        tm->working_cnt--;
        if (!tm->stop && tm->working_cnt == 0 && tm->work_first == NULL)
            // Avisamos a la funcion de espera para que se active
            pthread_cond_signal(&(tm->working_cond));
        pthread_mutex_unlock(&(tm->work_mutex));
    }

    // El hilo para de ejecutarse
    tm->thread_cnt--;
    pthread_cond_signal(&(tm->working_cond));
    pthread_mutex_unlock(&(tm->work_mutex));

    return NULL;
}

tpool_t* tpool_create(int num)
{
    tpool_t* tm;
    pthread_t thread;
    int i;

    if (num == 0) {
        num = 2;
    }

    tm = (tpool_t*)malloc(sizeof(tpool_t));
    if (!tm) {
        return NULL;
    }

    tm->thread_cnt = num;

    // Inicializamos los atributos de los hilos
    pthread_mutex_init(&(tm->work_mutex), NULL);
    pthread_cond_init(&(tm->work_cond), NULL);
    pthread_cond_init(&(tm->working_cond), NULL);

    // Inicializamos la cola de trabajo
    tm->work_first = NULL;
    tm->work_last = NULL;

    // Creamos los hilos
    for (i = 0; i < num; i++) {
        pthread_create(&thread, NULL, tpool_worker, tm);
        pthread_detach(thread);
    }

    return tm;
}

void tpool_destroy(tpool_t* tm)
{
    tpool_work_t* work = NULL;
    tpool_work_t* next = NULL;

    if (!tm) {
        return;
    }

    // Liberamos todos los trabajos de la cola de trabajos
    pthread_mutex_lock(&(tm->work_mutex));
    work = tm->work_first;
    while (work) {
        next = work->next;
        tpool_work_destroy(work);
        work = next;
    }

    // Enviamos la senial a todos los hilos para que terminen
    tm->stop = true;
    pthread_cond_broadcast(&(tm->work_cond));
    pthread_mutex_unlock(&(tm->work_mutex));

    // Esperamos a que terminen todos los hilos
    tpool_wait(tm);

    // Liberamos todos los atributos de los hilos
    pthread_mutex_destroy(&(tm->work_mutex));
    pthread_cond_destroy(&(tm->work_cond));
    pthread_cond_destroy(&(tm->working_cond));

    free(tm);
}

// Aniade un trabajo al pool de hilos
bool tpool_add_work(tpool_t* tm, thread_func_t func, void* arg)
{
    tpool_work_t* work = NULL;

    if (!tm) {
        return false;
    }

    work = tpool_work_create(func, arg);
    if (!work) {
        return false;
    }

    // Insertamos el trabajo en la cola de trabajos
    pthread_mutex_lock(&(tm->work_mutex));
    if (tm->work_first == NULL) {
        tm->work_first = work;
        tm->work_last = tm->work_first;
    } else {
        tm->work_last->next = work;
        tm->work_last = work;
    }
    // Avisamos de que hay trabajo que procesar
    pthread_cond_broadcast(&(tm->work_cond));
    pthread_mutex_unlock(&(tm->work_mutex));

    return true;
}

void tpool_wait(tpool_t* tm)
{
    if (!tm) {
        return;
    }

    pthread_mutex_lock(&(tm->work_mutex));
    while (1) {
        if ((!tm->stop && tm->working_cnt != 0) ||
            (tm->stop && tm->thread_cnt != 0)) {
            // Esperamos a que terminen todos los hilos
            pthread_cond_wait(&(tm->working_cond), &(tm->work_mutex));
        } else {
            break;
        }
    }
    pthread_mutex_unlock(&(tm->work_mutex));
}
