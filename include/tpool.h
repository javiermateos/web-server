/*****************************************************************************
 * ARCHIVO: tpool.h
 * DESCRIPCION: Interfaz de programacion para el pool de threads
 *
 * FECHA CREACION: 4 Marzo de 2021
 * AUTORES: Javier Mateos Najari, Adrian Sebastian Gil
 *****************************************************************************/
#ifndef __TPOOL_H__
#define __TPOOL_H__

#include <stdio.h>
#include <stdbool.h>

typedef struct tpool tpool_t; // Pool de hilos

typedef void (*thread_func_t)(void* arg); // Funcion tipo que ejecutan los hilos

/*******************************************************************************
 * FUNCION: tpool_t* tpool_create(int num)
 * ARGS_IN: int num - numero de hilos del pool.
 * DESCRIPCION: Crea e inicializa un pool de hilos.
 * ARGS_OUT: tpool_t* - pool de hilos inicializado.
 ******************************************************************************/
tpool_t* tpool_create(int num);

/*******************************************************************************
 * FUNCION: void tpool_destroy(tpool_t* tm)
 * ARGS_IN: int num - pool de hilos que se destruye.
 * DESCRIPCION: Destruye y libera el pool de hilos esperando a que todos los
 * hilos del pool finalicen.
 ******************************************************************************/
void tpool_destroy(tpool_t* tm);

/*******************************************************************************
 * FUNCION: bool tpool_add_work(tpool_t* tm, thread_func_t func, void* arg);
 * ARGS_IN: tpool_t* tm - pool de hilos al que se aniade el trabajo.
 *          thread_funct_t func - funcion de trabajo que ejecuta el hilo.
 *          void* arg - argumentos de la funcion de trabajo. 
 * DESCRIPCION: Aniade un trabajo a la cola de trabajos del pool de hilos. Si
 * no hay mas espacio en la cola de trabajo del pool la funcion bloquea al hilo
 * que la ha llamado hasta que haya espacio.
 * ARGS_OUT: bool - true si se aniade o false en caso contrario.
 ******************************************************************************/
bool tpool_add_work(tpool_t* tm, thread_func_t func, void* arg);

#endif /* __TPOOL_H__ */
