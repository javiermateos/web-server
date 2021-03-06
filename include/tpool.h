/*****************************************************************************
 * ARCHIVO: tpool.h
 * DESCRIPCION: Interfaz de programacion para el pool de threads
 *
 * FECHA CREACION: 4 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/
#ifndef __TPOOL_H__
#define __TPOOL_H__

#include <stdbool.h>

typedef struct tpool tpool_t;

typedef void (*thread_func_t)(void* arg);

tpool_t* tpool_create(int num);
void tpool_destroy(tpool_t* tm);

// Aniade trabajo a la cola
bool tpool_add_work(tpool_t* tm, thread_func_t func, void* arg);
void tpool_wait(tpool_t* tm);

#endif /* __TPOOL_H__ */
