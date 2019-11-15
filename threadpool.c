#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
/* inserire gli altri include che servono */
#include <pthread.h>
#include <icl_hash.h>
#include <stats.h>
#include <signal.h>
 
#include <threadpool.h>
#include <connections.h>
#include <parser.h>
#include <queue.h>

#define N 100

struct statistics mboxStats;

pthread_t lockedRepository = 0;

/* -------------------- Thread Jobs -------------------- */
void* threadFun(void* params1){
	message_data_t*			data;			// data del messaggio	
	message_data_t*			old;
	message_t* 				dataIn;			// 
	icl_entry_t*			esito;		
	membox_key_t* 			chiave;
	threadParams* 			params;
	op_t 					operation;
	long 					fd_c;			// fd della connessione da gestire
	int						ris;
	int						length;
	int 					handled_signal;
	pthread_t 				temp_lockedRepository;
	handled_signal = -1;

	params = (threadParams*) params1;
	/*	Ciclo infinito dei thread.
		Ogni thread prende una connessione dalla coda, legge messaggi da quella connessione, esegue l'operazione richiesta e invia la risposta.
    */
	while(1){
		// prelevo dalla coda
		fd_c = dequeue(params->queue);
		// se non ci sono connessioni in coda e ricevo -1 (è arrivato un segnale di terminazione) devo uscire e chiudere il thread.
		if (fd_c==-1){
			pthread_exit(NULL);
		}
		pthread_mutex_lock(&lockedStats);
		*(params->status) = RUNNING;
		mboxStats.concurrent_connections++;
		pthread_mutex_unlock(&lockedStats);
		
		dataIn = malloc(sizeof(message_t));
		// controllo errore allocazione
		if(dataIn == NULL){
			close(fd_c);
			// se la lock globale devo rilasciarla prima di uscire
			if(pthread_equal(lockedRepository,pthread_self())){
				pthread_mutex_lock(&globalLock);
				lockedRepository = 0;
				pthread_mutex_unlock(&globalLock);
				pthread_exit(NULL);
			}
		}

		// leggo una richiesta e provo a soddisfarla
		while(readRequest(fd_c, dataIn) != -1){
			int esiste_lock = pthread_equal(lockedRepository,0);
			int ho_lock = pthread_equal(lockedRepository,pthread_self());
			// se la lock globale è assegnata a qualcuno diverso da questo thread, allora non posso eseguire operazioni.
			if(esiste_lock==0 && ho_lock==0){
				operation = OP_LOCKED;
				write(fd_c, &operation, sizeof(op_t));
				write(fd_c, &(dataIn->hdr.key), sizeof(membox_key_t));
				free(dataIn->data.buf);
				break;
			}
			// calcolo l'hash della chiave, nel caso in cui debba acquisire la lock per accedere alla repository.
			int door = ulong_hash_function(&dataIn->hdr.key) % PRIME;
			switch(dataIn->hdr.op){

				case PUT_OP :
					// blocco le statistiche, controllo se ho la repository piena e non posso inserire il dato (troppi dati o dimensione massima raggiunta)
					pthread_mutex_lock(&lockedStats);
					if(mboxStats.current_objects>=params->config->StorageSize && params->config->StorageSize!=0){
						dataIn->hdr.op = OP_PUT_TOOMANY;
						sendHeader(fd_c, &(dataIn->hdr)); 
						pthread_mutex_unlock(&lockedStats);           
						free(dataIn->data.buf);
						break;
					}
					if(params->config->StorageByteSize!=0 && params->config->StorageByteSize < mboxStats.current_size + dataIn->data.len){
						dataIn->hdr.op = OP_PUT_REPOSIZE;
						sendHeader(fd_c, &(dataIn->hdr));  
						pthread_mutex_unlock(&lockedStats);          
						free(dataIn->data.buf);
						break;
					}
					if(params->config->MaxObjSize!=0 && params->config->MaxObjSize < dataIn->data.len){
						dataIn->hdr.op = OP_PUT_SIZE;
						sendHeader(fd_c, &(dataIn->hdr));  
						pthread_mutex_unlock(&lockedStats);          
						free(dataIn->data.buf);
						break;
					}
					pthread_mutex_unlock(&lockedStats);

					pthread_mutex_lock(&params->hold[door]);
					chiave = (membox_key_t*)malloc(sizeof(membox_key_t));
					if(chiave == NULL){
						free(dataIn);
						pthread_mutex_unlock(&params->hold[door]);
						pthread_exit(NULL);
					}
					*chiave = dataIn->hdr.key;
					data = malloc(sizeof(message_data_t));
					if(data == NULL){
						free(chiave);
						free(dataIn);
						pthread_mutex_unlock(&params->hold[door]);
						pthread_exit(NULL);
					}
					data->len = dataIn->data.len;
					data->buf = malloc(sizeof(char)*data->len);
					if(data->buf == NULL){
						free(data);
						free(chiave);
						free(dataIn);
						pthread_mutex_unlock(&params->hold[door]);
						pthread_exit(NULL);
					}
					memcpy(data->buf, dataIn->data.buf, dataIn->data.len);
					
					esito = icl_hash_insert(params->hash, chiave, data);
					if(esito!=NULL)
						dataIn->hdr.op = OP_OK;
					else
						dataIn->hdr.op = OP_PUT_ALREADY;
					pthread_mutex_unlock(&params->hold[door]);
					sendHeader(fd_c, &(dataIn->hdr));
					pthread_mutex_lock(&lockedStats);
					if(dataIn->hdr.op != OP_OK)
						mboxStats.nput_failed++;
					else{
						mboxStats.current_objects++;
						mboxStats.current_size += dataIn->data.len;
					}
					if(mboxStats.max_objects < mboxStats.current_objects)
						mboxStats.max_objects = mboxStats.current_objects;
					if(mboxStats.max_size < mboxStats.current_size)
						mboxStats.max_size = mboxStats.current_size;
					mboxStats.nput++;
					pthread_mutex_unlock(&lockedStats);
					free(dataIn->data.buf);
					break;

				case UPDATE_OP :
					pthread_mutex_lock(&params->hold[door]);
					// cerco elemento nella tabella hash
					esito = icl_hash_find(params->hash, &(dataIn->hdr.key));
					if(esito!=NULL){ // ho trovato l'elemento
						//controllo size data_old e data_new
						message_data_t* old = (message_data_t*) esito;
						// size diversa da quella originale
						if(dataIn->data.len != old->len)
							dataIn->hdr.op = OP_UPDATE_SIZE;
						//faccio update
						else{
							// elimino il vecchio elemento
							ris = icl_hash_delete(params->hash, &(dataIn->hdr.key), freeKey, freeData);
							if(ris == 0){ // cancellazione effettuata -> inserisco il nuovo elemento
								chiave = (membox_key_t*)malloc(sizeof(membox_key_t));
								if(chiave == NULL){
									free(dataIn);
									pthread_mutex_unlock(&params->hold[door]);
									pthread_exit(NULL);
								}
								*chiave = dataIn->hdr.key;
								data = malloc(sizeof(message_data_t));
								if(data == NULL){
									free(chiave);
									free(dataIn);
									pthread_mutex_unlock(&params->hold[door]);
									pthread_exit(NULL);
								}
								data->len = dataIn->data.len;
								data->buf = malloc(sizeof(char)*data->len);
								if(data->buf == NULL){
									free(data);
									free(chiave);
									free(dataIn);
									pthread_mutex_unlock(&params->hold[door]);
									pthread_exit(NULL);
								}
								memcpy(data->buf, dataIn->data.buf, dataIn->data.len);
								esito = icl_hash_insert(params->hash, chiave, data);
								if(esito!=NULL)
									dataIn->hdr.op = OP_OK;
								else
									dataIn->hdr.op = OP_FAIL;
							}else // fail cancellazione
								dataIn->hdr.op = OP_FAIL;
						}
					}
					else // se l'elemento non era presente
						dataIn->hdr.op = OP_UPDATE_NONE;
					write(fd_c, &(dataIn->hdr.op), sizeof(op_t));
					write(fd_c, &(dataIn->hdr.key), sizeof(membox_key_t));
					pthread_mutex_unlock(&params->hold[door]);

					pthread_mutex_lock(&lockedStats);
					if(dataIn->hdr.op != OP_OK)
						mboxStats.nupdate_failed++;
					mboxStats.nupdate++;
					pthread_mutex_unlock(&lockedStats);
					free(dataIn->data.buf);
					break;

				case GET_OP :
					pthread_mutex_lock(&params->hold[door]);
					// cerco l'elemento
					esito = icl_hash_find(params->hash, &(dataIn->hdr.key));
					old = (message_data_t*) esito;
					if(old!=NULL){
						// se ho trovato l'elemento
						dataIn->hdr.op = OP_OK;
						dataIn->data.len = old->len;
						free(dataIn->data.buf);
						dataIn->data.buf = malloc(sizeof(char)*dataIn->data.len);
						if(dataIn->data.buf == NULL){
							free(dataIn);
							pthread_mutex_unlock(&params->hold[door]);
							pthread_exit(NULL);
						}
						memcpy(dataIn->data.buf, old->buf, dataIn->data.len);
					}
					else
						dataIn->hdr.op = OP_GET_NONE;
					sendRequest(fd_c, dataIn);
					pthread_mutex_unlock(&params->hold[door]);
					
					pthread_mutex_lock(&lockedStats);
					if(dataIn->hdr.op != OP_OK)
						mboxStats.nget_failed++;
					mboxStats.nget++;
					pthread_mutex_unlock(&lockedStats);
					free(dataIn->data.buf);
					break;

				case REMOVE_OP : 
					pthread_mutex_lock(&params->hold[door]);
					esito = icl_hash_find(params->hash, &(dataIn->hdr.key));
					old = (message_data_t*) esito;
					if (old!=NULL){
						length = old->len;
					
						if(icl_hash_delete(params->hash, &(dataIn->hdr.key), freeKey, freeData) == 0)
							// se sono riuscito a rimuovere l'elemento
							dataIn->hdr.op = OP_OK;
						else
							dataIn->hdr.op = OP_REMOVE_NONE;
					}
					else
						dataIn->hdr.op = OP_REMOVE_NONE;
						
					sendHeader(fd_c, &(dataIn->hdr));
					pthread_mutex_unlock(&params->hold[door]);

					pthread_mutex_lock(&lockedStats);
					if(dataIn->hdr.op != OP_OK)
						mboxStats.nremove_failed++;
					else{
						mboxStats.current_objects--;
						mboxStats.current_size -= length;
				
					}
					mboxStats.nremove++;
					pthread_mutex_unlock(&lockedStats);
					free(dataIn->data.buf);
					break;

				case LOCK_OP :
					pthread_mutex_lock(&globalLock);
					
					if(lockedRepository == 0){
						operation = OP_OK;
						lockedRepository = pthread_self();
						write(fd_c, &operation, sizeof(op_t));}

					else{
						operation = OP_LOCKED;
						write(fd_c, &operation, sizeof(op_t));}
					
					write(fd_c, &(dataIn->hdr.key), sizeof(membox_key_t)); 
					pthread_mutex_unlock(&globalLock);

					pthread_mutex_lock(&lockedStats);
					if(dataIn->hdr.op != OP_OK)
						mboxStats.nlock_failed++;
					mboxStats.nlock++;
					pthread_mutex_unlock(&lockedStats);
					free(dataIn->data.buf);
					break;

				case UNLOCK_OP :
					pthread_mutex_lock(&globalLock);

					if(lockedRepository == pthread_self()){
						lockedRepository = 0;
						operation = OP_OK;
						write(fd_c, &operation, sizeof(op_t));}

					else if (lockedRepository == 0){
						operation = OP_LOCK_NONE;
						write(fd_c, &operation, sizeof(op_t));}
				
					else{
						operation = OP_LOCKED;
						write(fd_c, &operation, sizeof(op_t));}

					write(fd_c, &(dataIn->hdr.key), sizeof(membox_key_t)); 
					pthread_mutex_unlock(&globalLock);
					free(dataIn->data.buf);
					break;
		
				default :
					fprintf(stderr, "ERRORE: operazione non definita.\n");
					dataIn->hdr.op=OP_FAIL;
					sendHeader(fd_c, &(dataIn->hdr));
					free(dataIn->data.buf);
					break;

			} // fine switch
			/*	controllo SIGINT, SIGQUIT e SIGTERM
				se ho ricevuto un segnale di terminazione immediata, dealloco ed esco */
			if(handled_signal==SIGINT || handled_signal==SIGQUIT || handled_signal==SIGTERM){
				close(fd_c);
				// se la lock globale devo rilasciarla prima di uscire
				if(pthread_equal(lockedRepository,pthread_self())){
					pthread_mutex_lock(&globalLock);
					lockedRepository = 0;
					pthread_mutex_unlock(&globalLock);}
				free(dataIn);
				pthread_exit(NULL);
			}
		} // fine while di lettura messaggi dalla connessione
		close(fd_c);
		free(dataIn);
		// se ho ancora la lock globale dopo che la connessione si è conclusa, devo rilasciarla
		temp_lockedRepository = lockedRepository;
		if(pthread_equal(temp_lockedRepository,pthread_self())){
			pthread_mutex_lock(&globalLock);
			lockedRepository = 0;
			pthread_mutex_unlock(&globalLock);
		}
		pthread_mutex_lock(&lockedStats);
		mboxStats.concurrent_connections--;
		*(params->status) = WAITING;
		pthread_mutex_unlock(&lockedStats);
	} // fine while di prelievo connessioni dalla coda
	return NULL;
}


/* -------------------- Make Pool -------------------- */
threadPool* makePool(queue_t* queue, conf* settings, icl_hash_t* hash, pthread_mutex_t* hold){
	threadPool*						pool;			// pool di thread
	threadParams* 					params;			// parametri da passare alla funzione dei thread
	int								esito;			// esito creazione thread
	int 							i;
	pthread_t 						lockedRepository;
	lockedRepository = 0;


	pool = (threadPool*)malloc(sizeof(threadPool));
	if(pool == NULL)
		return NULL;
	params = (threadParams*)malloc(sizeof(threadParams));
	if(params == NULL){
		free(pool);
		return NULL;
	}
	
	pool->queue = queue;

	pool->threads = malloc(sizeof(pthread_t)*settings->ThreadsInPool); 
	if(pool->threads == NULL){
		free(pool);
		free(params);
		return NULL;
	}
	pool->status = malloc(sizeof(status_t)*settings->ThreadsInPool);
	if(pool->status == NULL){
		free(pool->threads);
		free(pool);
		free(params);
		return NULL;
	}
	pool->config = settings;
  	pool->params = params;

	params->queue = queue;
	params->hash = hash;
	params->hold = hold;
	params->config = settings;
	
	// creo settings->ThreadsInPool thread
	pthread_mutex_lock(&lockedStats);
	for(i=0; i<settings->ThreadsInPool; i++){
		pool->status[i]=WAITING;
		/*	params.status è il puntatore all'i-esimo elemento nell'array pool.status (l'array di stato dei thread) 
        	e rappresenta quindi lo stato dell'i-esimo thread (0 = wait, 1 = run) */
		params->status = &(pool->status[i]);
		esito = pthread_create(&(pool->threads[i]), NULL, threadFun, (void*)params);
		// if errore allocazione
		if(esito != 0){
           	handled_signal=SIGKILL;
			return NULL;
		}
	}
	pthread_mutex_unlock(&lockedStats);
	return pool;
}



/* -------------------- Running Threads -------------------- */
int nRunning(threadPool* pool){
	int sum = 0;
	int i;
	pthread_mutex_lock(&lockedStats);
	for (i=0; i<(pool->config->ThreadsInPool); i++)
		if(pool->status[i]==RUNNING)
			sum++;
	pthread_mutex_unlock(&lockedStats);
	return sum;
}

void freeKey(void* key){
	membox_key_t* temp = (membox_key_t*) key;
	free(temp);
}


/* -------------------- Free Data -------------------- */
void freeData(void* data){
	message_data_t* temp = (message_data_t*) data;
	free(temp->buf);
	free(temp);
}

/* ---------------------------------------------------------------------- 
 * Hashing funtions
 * Well known hash function: Fowler/Noll/Vo - 32 bit version
 */
unsigned int fnv_hash_function( void *key, int len ) {
	unsigned char *p = (unsigned char*)key;
	unsigned int h = 2166136261u;
	int i;
	for ( i = 0; i < len; i++ )
		h = ( h * 16777619 ) ^ p[i];
	return h;
}
unsigned int ulong_hash_function( void *key ) {
	int len = sizeof(unsigned long);
	unsigned int hashval = fnv_hash_function( key, len );
	return hashval;
}
int ulong_key_compare( void *key1, void *key2  ) {
	return ( *(unsigned long*)key1 == *(unsigned long*)key2 );
}

