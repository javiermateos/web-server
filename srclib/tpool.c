/*****************************************************************************
 * ARCHIVO: tpool.c
 * DESCRIPCION: Implementacion de la interfaz del pool de threads
 *
 * ADAPTACION: https://nachtimwald.com/2019/04/12/thread-pool-in-c/
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
    pthread_cond_t working_cond; // Indica que un thread ha finalizado
    pthread_cond_t gap_cond;     // Indica que hay sitio en la cola
    size_t working_cnt;          // Indica cuantos threads hay trabajando
    size_t thread_cnt;           // Indica cuantos threads estan vivos
    size_t queue_size;           // Indica el tamanio max de la cola de trabajo
    size_t work_cnt;             // Indica el tamanio actual de la cola
    bool stop;                   // Para los hilos
};

/*******************************************************************************
 * FUNCION: static tpool_work_t* tpool_work_create(thread_func_t func, void* arg);
 * ARGS_IN: thread_func_t func - funcion de trabajo que ejecutara el hilo.
 *          void* arg - argumento de la funcion de trabajo.
 * DESCRIPCION: Crea e inicializa un trabajo.
 * ARGS_OUT: tpool_work_t* - trabajo inicializado.
 ******************************************************************************/
static tpool_work_t* tpool_work_create(thread_func_t func, void* arg);

/*******************************************************************************
 * FUNCION: static void tpool_work_destroy(tpool_work_t* work);
 * ARGS_IN: tpool_work_t* work - trabajo que se destruye.
 * DESCRIPCION: Libera y destruye un trabajo y sus recursos asociados.
 ******************************************************************************/
static void tpool_work_destroy(tpool_work_t* work);

/*******************************************************************************
 * FUNCION: static tpool_work_t* tpool_get_work(tpool_t* tm);
 * ARGS_IN: tpool_t* tm - Pool de hilos del que se obtiene el trabajo.
 * DESCRIPCION: Obtiene un trabajo de la cola de trabajos de un pool de hilos.
 * ARGS_OUT: tpool_work_t* - trabajo extraido de la cola de trabajo.
 ******************************************************************************/
static tpool_work_t* tpool_get_work(tpool_t* tm);

/*******************************************************************************
 * FUNCION: static void* tpool_worker(void* arg);
 * ARGS_IN: void* arg - contiene los argumentos de la funcion
 * DESCRIPCION: Funcion que es ejecutada por todos los hilos y donde el trabajo
 * es realizado por los mismos. Esta funcion espera hasta que haya trabajo que
 * procesar y lo procesa.
 * ARGS_OUT: void* - NULL si todo ha ido correctamente.
 ******************************************************************************/
static void* tpool_worker(void* arg);

/*******************************************************************************
 * FUNCION: void tpool_wait(tpool_t* tm);
 * ARGS_IN: tpool_t* tm - Pool de hilos al que se espera que finalice.
 * DESCRIPCION: Funcion que espera a que todos los hilos del pool finalicen
 * antes de retornar.
 ******************************************************************************/
void tpool_wait(tpool_t* tm);

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

    // Avisamos de que hay espacio en la cola
    tm->work_cnt--;
    pthread_cond_signal(&(tm->gap_cond));

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
            // El trabajo es ejecutado por el hilo
            work->func(work->arg);
            tpool_work_destroy(work);
        }

        // El trabajo ha sido procesado
        pthread_mutex_lock(&(tm->work_mutex));
        tm->working_cnt--;
        if (!tm->stop && !tm->working_cnt && !tm->work_first) {
            // Avisamos a la funcion de espera para que se active
            pthread_cond_signal(&(tm->working_cond));
        }
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
    tpool_t* tm = NULL;
    pthread_t thread;
    int i;

    if (!num) {
        num = 1;
    }

    tm = (tpool_t*)calloc(1, sizeof(tpool_t));
    if (!tm) {
        return NULL;
    }

    tm->thread_cnt = num;
    tm->queue_size = num;

    // Inicializamos los atributos de los hilos
    pthread_mutex_init(&(tm->work_mutex), NULL);
    pthread_cond_init(&(tm->work_cond), NULL);
    pthread_cond_init(&(tm->working_cond), NULL);
    pthread_cond_init(&(tm->gap_cond), NULL);

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
    pthread_cond_destroy(&(tm->gap_cond));

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
    while (tm->work_cnt >= tm->queue_size) {
        // Esperamos hasta que haya espacio en la cola de trabajo
        pthread_cond_wait(&(tm->gap_cond), &(tm->work_mutex));
    }
    tm->work_cnt++;
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
