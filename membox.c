/*
 * membox Progetto del corso di LSO 2016 
 *
 * 
 */
/**
 * @file membox.c
 * @brief File principale del server membox
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>

/* inserire gli altri include che servono */
#include <stats.h>
#include <icl_hash.h>/*
#include <parser.h>
#include <queue.h>*/
#include <threadpool.h>


#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

/* struttura che memorizza le statistiche del server, struct statistics 
 * e' definita in stats.h.
 *
 */
struct statistics  mboxStats = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

void* sig_handler( void* arg );

FILE* file;

/* -------------------------------------------------- Server Main -------------------------------------------------- */
int main(int argc, char *argv[]) {
	icl_hash_t*				hash;			// tabella hash
	FILE*					config;			// file di configurazione
	conf*					settings;		// parametri letti dal file di configurazione
	queue_t* 				connections;	// coda di connessioni create con la accept
	int						i;
	
	struct sockaddr_un		client;
	struct sockaddr*		client_ptr;
	socklen_t				client_len;
	int						conn;			// FD della connessione
	int						fd; 			// FD socket
	struct sockaddr_un 		sa;				// struct della bind

	threadPool* 			pool;
	pthread_mutex_t*		hold;
	
	handled_signal=-1;
	/* ------------------------- Maschera Segnali ------------------------- */
	pthread_t				sig_thread;
	struct					sigaction siga;

	// Creo maschera vuota e aggiungo i segnali che mi interessano 
	siga.sa_handler=SIG_IGN;
	sigemptyset(&siga.sa_mask);
	sigaddset(&siga.sa_mask,SIGUSR1);
	sigaddset(&siga.sa_mask,SIGUSR2);
	sigaddset(&siga.sa_mask,SIGINT);
	sigaddset(&siga.sa_mask,SIGTERM);
	sigaddset(&siga.sa_mask,SIGQUIT);
	sigaddset(&siga.sa_mask,SIGPIPE);
	pthread_sigmask( SIG_BLOCK, &siga.sa_mask, NULL);
		

	/* ------------------------- configFile Parsing ------------------------- */
	if(strcmp(argv[1], "-f")!=0){
		printf("Manca il flag -f.\n");
		return -1;
	}
	config = fopen(argv[2], "r");
	if(!config){
		printf("Il secondo parametro deve essere un file di configurazione!\n");
		return -1;
	}
	settings = parseConfig(config);
	fclose(config);
	if(settings==NULL){
		fprintf(stderr, "Errore nell'allocazione del file dei parametri di configurazione.\n");
		return -1;
	}


	/* ------------------------- Tabella Hash ------------------------- */
	hash = icl_hash_create(PRIME, ulong_hash_function, ulong_key_compare);
	if(hash == NULL){
		free(settings);
		fprintf(stderr, "Errore nell'allocazione della repository.\n");
		return -1;
	}



	/* ------------------------- Array Mutex ------------------------- */ 
	hold = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t)*PRIME);
	if(hold == NULL){
		free(settings);
		icl_hash_destroy(hash, freeKey, freeData);
		fprintf(stderr, "Errore nell'allocazione dell'array di mutex.\n");
		return -1;
	}

	for(i=0; i<PRIME; i++){
		pthread_mutex_init(&(hold[i]), NULL);
	}
	
	/* ------------------------- Creo Socket ------------------------- */
	strncpy(sa.sun_path, settings->UnixPath, UNIX_PATH_MAX);
	sa.sun_family=AF_UNIX;

	fd = socket(AF_UNIX,SOCK_STREAM,0);
	if(fd == -1){
		free(settings);
		icl_hash_destroy(hash, freeKey, freeData);
		for(i=0; i<PRIME; i++)
			pthread_mutex_destroy(&hold[i]);
		free(hold);
		fprintf(stderr, "Errore nella creazione della socket.\n");
		return -1;
	}
	bind(fd,(struct sockaddr*) &sa, sizeof(sa));
	listen(fd, SOMAXCONN);	// --> SOMAXCONN = max richieste accodabili

	client_ptr = (struct sockaddr*) &client;



	/* ------------------------- Coda Connessioni ------------------------- */
	connections = createQueue(settings->MaxConnections);
	if(connections == NULL){
		free(settings);
		icl_hash_destroy(hash, freeKey, freeData);
		for(i=0; i<PRIME; i++)
			pthread_mutex_destroy(&hold[i]);
		free(hold);
		close(fd);
		fprintf(stderr, "Errore nella creazione della coda di connessioni.\n");
		return -1;
	}
	
	
	/* ------------------------- Thread Pool -------------------------- */	
	pool = makePool(connections, settings, hash, hold);
	if(pool == NULL){
		free(settings);
		icl_hash_destroy(hash, freeKey, freeData);
		for(i=0; i<PRIME; i++)
			pthread_mutex_destroy(&hold[i]);
		free(hold);
		close(fd);
		freeQueue(connections);
		fprintf(stderr, "Errore nella creazione del pool di thread.\n");
		return -1;
	}
	

	file = fopen(settings->StatFileName, "w+");

	/* ------------------------- Gestore Segnali ------------------------- */
	/*	Provo a creare il thread handler per un massimo di 10 volte.
    	In caso di fallimento devo chiudere tutto, in quanto un handler
        è necessario per la corretta terminazione del processo.
    */
	int nVolte = 0;
	while(pthread_create( &sig_thread, NULL, sig_handler, (void*) &fd )!=0 && nVolte!=10)
    	nVolte++;
	if(nVolte==10){
    	printf("Errore nella creazione del thread handler.\n");
		handled_signal = SIGKILL;
	}
	
	/* ------------------------- Dispatcher -------------------------- */	
	client_len = sizeof(client_ptr);

	/* 	mi metto in attesa della connessione, se ne trovo una controllo se ho già abbastanza connessioni attive.
		se non ne ho, allora la aggiungo nella coda delle connessioni da soddisfare, altrimenti la rifiuto*/
	while(1){		
    	// controllo se ho ricevuto segnali
    	if(handled_signal==-1){
			conn = accept(fd, (struct sockaddr *) &client, &client_len);
			// potrei aver rivecuto segnali nel frattempo! --> non potremmo evitare il doppio controllo con una lock?
			if(handled_signal==-1){						
				if(conn!=-1){
					if(nRunning(pool) + connections->nElem < connections->maxElem){
						enqueue(connections, conn);
					}
					else{
						// rifiuta la connessione
						op_t op = OP_FAIL;
						write(conn, &op, sizeof(op_t));
						long key = 0;
						write(conn, &key, sizeof(long));
					}
				}
				else
					// errore
					break;
			}// ho un segnale (dopo la accept)
			else
				break;
		}
		// ho un segnale (prima della accept)
		else
			break;
	}
	printf("Terminazione in corso...\n");
	// aspetto che tutti i thread finiscano di eseguire operazioni e si arrestino correttamente
	for (i=0;i<pool->config->ThreadsInPool; i++)
		pthread_join(pool->threads[i], NULL);
	pthread_join(sig_thread, NULL);

	close(fd);
	printStats(file);
	fclose(file);
 
	// distruggo lock globali
	pthread_mutex_destroy(&globalLock);
	//pthread_mutex_destroy(&repo);
	pthread_mutex_destroy(&lockedStats);
	pthread_mutex_destroy(&qlock);
	pthread_mutex_destroy(&sig_mutex);
	pthread_cond_destroy(&varCond);
	
	for(i=0; i<PRIME; i++)
		pthread_mutex_destroy(&hold[i]);
	free(hold);
	printf("Lock rimosse correttamente.\n");
  
	// libero la memoria allocata
	icl_hash_destroy(hash, freeKey, freeData);
	freeQueue(pool->queue);
	free(pool->status);
	free(pool->threads);
	free(pool->params);
	free(pool);
	free(settings->UnixPath);
	free(settings->StatFileName);
	free(settings);
  
	pthread_exit(NULL);  
	return 0;
}


void* sig_handler( void* fd1 ){
	sigset_t signal_set;
	int sig;
	int fd = *((int*)fd1);

	for(;;) {
		sigfillset( &signal_set );
		sigwait( &signal_set, &sig );

		switch(sig){
			/*	ho ricevuto SIGUSR1, devo solo stampare le statistiche */
			case SIGUSR1:
				pthread_mutex_lock(&lockedStats);
				printStats(file);
				pthread_mutex_unlock(&lockedStats);
				handled_signal=-1;

				break;
				
			/*	ho ricevuto SIGUSR2, stampo le statistiche e segnalo a tutti i thread */
			case SIGUSR2:
				shutdown(fd, SHUT_RD);				
				handled_signal = SIGUSR2;
				pthread_cond_broadcast(&varCond);
				pthread_exit(NULL);
				break;
				
			/*	ho ricevuto SIGINT, segnalo ai thread di chiudersi subito */
			 case SIGINT:
				shutdown(fd, SHUT_RD);
				handled_signal = SIGINT;
				pthread_cond_broadcast(&varCond);
				pthread_exit(NULL);
				break;
				
			/* 	ho ricevuto SIGTERM,segnalo ai thread di chiudersi subito */
			 case SIGTERM:
				shutdown(fd, SHUT_RD);
				handled_signal = SIGTERM;
				pthread_cond_broadcast(&varCond);
				pthread_exit(NULL);
				break;
				
			/*	ho ricevuto SIGQUIT, segnalo ai thread di chiudersi subito */
			 case SIGQUIT:
				shutdown(fd, SHUT_RD);
				handled_signal = SIGQUIT;
				pthread_cond_broadcast(&varCond);
				pthread_exit(NULL);
				break;

			/*	ho ricevuto un segnale che non mi interessa, non lo gestisco */
			default:
				break;
		}
	}
	return NULL;
}



