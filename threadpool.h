#ifndef THREADPOOL
#define THREADPOOL
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
/* inserire gli altri include che servono */
#include <pthread.h>

#include <icl_hash.h>
#include <connections.h>
#include <parser.h>
#include <queue.h>

#define PRIME 2111

/**
 * @file  threadpool.h
 * @brief Contiene la struttura dati implementativa del pool di thread worker e la specifica delle loro funzioni
 */

extern pthread_t lockedRepository;
static pthread_mutex_t globalLock = PTHREAD_MUTEX_INITIALIZER;
//static pthread_mutex_t repo = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lockedStats = PTHREAD_MUTEX_INITIALIZER;


/* ------------------------- Strutture Dati ------------------------- */
 /**
 *  @enum status_t
 *  @brief flag dello stato del thread worker
 *
 */
typedef enum{
	WAITING,								///< thread in stato di attesa
	RUNNING									///< thread in stato di esecuzione
} status_t;

 /**
 *  @struct threadParams
 *  @brief parametri della funzione iniziale dei thread worker
 *
 */
typedef struct{
	queue_t*				queue;			///< coda delle connessioni 
	icl_hash_t*				hash;			///< tabella hash
	status_t*				status;			///< array di stati dei thread nel pool
	pthread_mutex_t*		hold;			///< array di mutex della repository
	conf*					config;			///< insieme dei parametri di configurazione
} threadParams;

 /**
 *  @struct threadPool
 *  @brief tipo del pool di thread
 *
 */
typedef struct{	
	pthread_t*				threads;		///< array di thread
	queue_t*				queue;			///< coda delle connessioni
	status_t* 				status;			///< array di stati dei thread nel pool
	pthread_mutex_t*		hold;			///< array di mutex della repository
	conf*					config;			///< insieme dei parametri di configurazione
  	threadParams*			params;			///< parametri dei thread worker
} threadPool;


/* -------------------- Thread Jobs -------------------- */
/**
 * @function threadFun
 * @brief Funzione iniziale dei thread worker.
 *
 * @param params1 Path del socket AF_UNIX 
 *
 * @return NULL in caso di chiusura
 */
void* threadFun(void* params1);


/* -------------------- Make Pool -------------------- */
/**
 * @function makePool
 * @brief Crea un pool di thread worker.
 *
 * @param queue coda delle connessioni
 * @param settings struttura dati contenente i parametri di configurazione
 * @param hash tabella hash contenente la repository
 * @param hold array di mutex della repository
 *
 * @return il puntatore al pool di thread in caso di successo
 * @return NULL in caso di errore
 */
threadPool* makePool(queue_t* queue, conf* settings, icl_hash_t* hash, pthread_mutex_t* hold);


/* -------------------- Running Threads -------------------- */
/**
 * @function nRunning
 * @brief Calcola il numero di thread che stanno soddisfando una connessione
 *
 * @param pool il pool di thread worker
 *
 * @return il numero di thread attualmente in esecuzione
 */
int nRunning(threadPool* pool);


/* -------------------- Free Data -------------------- */
/**
 * @function freeData
 * @brief Dealloca il campo data di un messaggio
 *
 * @param data il puntatore al dato da liberare
 *
 * @return void
 */
void freeData(void* data);


/* -------------------- Free Key -------------------- */
/**
 * @function freeKey
 * @brief Dealloca il campo key di un messaggio
 *
 * @param key il puntatore alla chiave da liberare
 *
 * @return void
 */
void freeKey(void* key);


/* -------------------- Hashing Functions -------------------- */
/*
 * Funzioni hash note
 */
unsigned int fnv_hash_function( void *key, int len );
unsigned int ulong_hash_function( void *key );
int ulong_key_compare( void *key1, void *key2  );
#endif

