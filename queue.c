#include <stdio.h>
#include <stdlib.h>
#include <queue.h>
#include <pthread.h>
#include <signal.h>

int handled_signal;
pthread_cond_t varCond = PTHREAD_COND_INITIALIZER;


/* -------------------- Funzioni -------------------- */
int isEmpty(queue_t* coda){
	LockQueue();
	int res = coda->nElem==0;
	UnlockQueue();
	return res;
}

queue_t* createQueue(int maxElem){
	handled_signal=-1;
	queue_t* coda;
	coda = (queue_t*)malloc(sizeof(queue_t));
	if(coda == NULL)
		return NULL;
	coda->queue = (long*)malloc(maxElem*sizeof(long));
	if(coda->queue == NULL){
		free(coda);
		return NULL;
	}
	coda->maxElem = maxElem;
	coda->nElem = 0;
	coda->head = -1;
	coda->tail = 0;

	return coda;
}

void freeQueue(queue_t* coda){
	LockQueue();
	free(coda->queue);
	free(coda);
	UnlockQueue();
}

int enqueue(queue_t* coda, long elem){
	LockQueue();
	if(coda->nElem == coda->maxElem){
		UnlockQueue();
		return -1;
	}
	// se la lista è vuota
	if(coda->nElem == 0){
		coda->head = 0;
		coda->tail = 1;
		coda->nElem++;
		// inserisco elemento
		coda->queue[coda->head] = elem;
		UnlockQueue();
    	// faccio una signal per svegliare i thread in attesa sulla coda vuota
		pthread_cond_signal(&varCond);
		return 0;
	}
	coda->queue[coda->tail] = elem;
	coda->tail++;
	// se sono in fondo all'array, allora ritorno in cima  
	if(coda->tail == coda->maxElem)
		coda->tail -= coda->maxElem;
	coda->nElem++;
	UnlockQueue();
	return 0;
}

long dequeue(queue_t* coda){
	long elem;
	LockQueue();

    // se la coda è vuota e non ho segnali aspetto connessioni, altrimenti restituisco -1
	while(coda->nElem == 0){
	    if(handled_signal!=-1){
			UnlockQueue();
	    	return -1;
	    }
	    else
			pthread_cond_wait(&varCond, &qlock);
	}

	elem = coda->queue[coda->head];
	coda->head++;
	// se sono in fondo all'array, allora ritorno in cima  
	if(coda->head == coda->maxElem)
		coda->head -= coda->maxElem;
	coda->nElem--;
	UnlockQueue();
	return elem;
}
