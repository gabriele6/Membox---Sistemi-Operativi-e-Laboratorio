#ifndef QUEUE
#define QUEUE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>


/**
 * @file  queue.h
 * @brief Contiene la struttura dati implementativa della coda di connessioni e la specifica delle sue funzioni
 */

static pthread_mutex_t qlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t sig_mutex = PTHREAD_MUTEX_INITIALIZER;
extern pthread_cond_t varCond;
extern int handled_signal;


/* -------------------- Struttura Dati -------------------- */
 /**
 *  @struct queue_t
 *  @brief tipo della coda di connessioni
 *
 */
typedef struct{
	long*	queue;			///< array contenente i File Descriptor delle connessioni accettate
	int 	nElem;			///< numero di elementi attualmente in coda
	int 	maxElem;		///< massimo numero di elementi accodabili
	int		head;			///< puntatore al primo elemento della coda
	int 	tail;			///< puntatore all'ultimo elemento della coda
} queue_t;


/* -------------------- isEmpty -------------------- */
/**
 * @function isEmpty
 * @brief Controlla se la coda è vuota
 *
 * @param coda il puntatore alla coda da controllare
 *
 * @return 0 se la coda non è vuota, 1 altrimenti
 */
int isEmpty(queue_t* coda);


/* -------------------- createQueue -------------------- */
/**
 * @function createQueue
 * @brief Crea una coda di maxElem posizioni libere
 *
 * @param maxElem intero che rappresenta il massimo numero di elementi nella coda
 *
 * @return il puntatore alla coda creata
 */
queue_t* createQueue(int maxElem);



/* -------------------- freeQuueue -------------------- */
/**
 * @function freeQueue
 * @brief Libera una coda allocata
 *
 * @param coda il puntatore alla coda da liberare
 *
 * @return	void
 */
void freeQueue(queue_t* coda);


/* -------------------- enqueue -------------------- */
/**
 * @function enqueue
 * @brief Aggiunge un elemento alla coda
 *
 * @param coda il puntatore alla coda sulla quale aggiungere l'elemento
 * @param elem l'elemento da aggiungere in coda
 *
 * @return 0 se l'inserimento è andato a buon fine
 * @return -1 altrimenti
 */
int enqueue(queue_t* coda, long elem);


/* -------------------- dequeue -------------------- */
/**
 * @function dequeue
 * @brief Estrae un elemento da una coda (o si mette in attesa di elementi se non ce ne sono)
 *
 * @param coda il puntatore alla coda
 *
 * @return -1 se la coda è vuota e ho ricevuto un segnale
 * @return il valore estratto altrimenti
 */
long dequeue(queue_t* coda);


/* -------------------- LockQueue -------------------- */
/**
 * @function LockQueue
 * @brief Blocca la coda per poter eseguire operazioni in mutua esclusione
 */
static void LockQueue(){ pthread_mutex_lock(&qlock); }


/* -------------------- UnlockQueue -------------------- */
/**
 * @function UnlockQueue
 * @brief Sblocca la coda
 */
static void UnlockQueue()          { pthread_mutex_unlock(&qlock); }

#endif

