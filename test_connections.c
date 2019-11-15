// thread 1: connessione, crea messaggi (funzione prof), legge risposte
// thread 2.1 legge messaggi, controlla che sia corretto con funzione prof, scrive messaggio
// thread 2.2 dopo che thread2.1 funziona: connessione, legge messaggi, esegue le OP di threadpool, risponde

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#define UNIX_PATH_MAX 108
#define SOCKNAME "./mysock"
#define NUM 1000
#define NN 100

#include <assert.h>


typedef enum {

    /* ----------------------------------- */
    /*      operazioni da implementare     */
    /* ------------------------------------*/
    PUT_OP          = 0,   /// inserimento di un oggetto nel repository
    UPDATE_OP       = 1,   /// aggiornamento di un oggetto gia' presente
    GET_OP          = 2,   /// recupero di un oggetto dal repository
    REMOVE_OP       = 3,   /// eliminazione di un oggetto dal repository
    LOCK_OP         = 4,   /// acquisizione di una lock su tutto il repository
    UNLOCK_OP       = 5,   /// rilascio della lock su tutto il repository

    /* ----------------------------------- */
    /* ----------------------------------- */

    END_OP          = 6,   // numero di operazioni definite    

    /* ----------------------------------- */
    /*    messaggi di ritorno dal server   */
    /* ------------------------------------*/

    OP_OK           = 11,  // operazione eseguita con successo    
    OP_FAIL         = 12,  // generico messaggio di fallimento
    OP_PUT_ALREADY  = 13,  // put di un oggetto gia' presente nel repository 
    OP_PUT_TOOMANY  = 14,  // raggiunto il massimo numero di oggetti
    OP_PUT_SIZE     = 15,  // put di un oggetto troppo grande
    OP_PUT_REPOSIZE = 16,  // superata la massima size del repository
    OP_GET_NONE     = 17,  // get di un oggetto non presente
    OP_REMOVE_NONE  = 18,  // rimozione di un oggetto non presente
    OP_UPDATE_SIZE  = 19,  // size dell'oggetto da aggiornare non corrispondente
    OP_UPDATE_NONE  = 20,  // update di un oggetto non presente
    OP_LOCKED       = 21,  // operazione non permessa perche' il sistema e' in stato di lock
    OP_LOCK_NONE    = 22,  // richiesta di unlock ma il sistema non e' in stato di lock
    OP_BUSY         = 23   // raggiunto il numero massimo di connessioni
    /* 
     * aggiungere qui altri messaggi di ritorno che possono servire 
     */

} op_t;

typedef unsigned long membox_key_t;
typedef struct {
    op_t           op;   
    membox_key_t   key;  
} message_hdr_t;
typedef struct {
    unsigned int   len;  
    char          *buf;
} message_data_t;
typedef struct {
    message_hdr_t  hdr;
    message_data_t data;
} message_t;

void setKey(membox_key_t *key_out, membox_key_t *key_in) {
    assert(key_out); 
    assert(key_in);
    *key_out = *key_in;   
}

void setHeader(message_t *msg, op_t op, membox_key_t *key) {
#if defined(MAKE_VALGRIND_HAPPY)
    ((long*)&(msg->hdr))[0] = 0;
    ((long*)&(msg->hdr))[1] = 0;
#endif
    msg->hdr.op  = op;
    setKey(&msg->hdr.key, key);
}

void setData(message_t *msg, const char *buf, unsigned int len) {
    msg->data.len = len;
    msg->data.buf = (char *)buf;
}

int sendRequest(long fd, message_t *msg){
	int temp = 0;
	int total = 0;

	if(write(fd, &(msg->hdr.op), sizeof(op_t)))
		if(write(fd, &(msg->hdr.key), sizeof(membox_key_t))){
			if(write(fd, &(msg->data.len), sizeof(unsigned int))){
				total = msg->data.len;
             	/* controllo di aver letto tutto il buffer */
				while((temp = write(fd, (msg->data.buf)+temp, total)) < total){
					if(temp == -1){
						errno = ECOMM;
	                   	return -1;
					}
					total -= temp;
				}	
				return 0;
			}
		}
	errno = ECOMM;
	return -1;
}

int readReply(long fd, message_t *msg){
	int temp = 0;
	int total = 0;

	if(read(fd, &(msg->hdr.op), sizeof(op_t)) >= sizeof(op_t))												
		if(read(fd, &(msg->hdr.key), sizeof(membox_key_t))>=sizeof(membox_key_t))
			if(read(fd, &(msg->data.len), sizeof(unsigned int))>=sizeof(unsigned int)){					
        		msg->data.buf = malloc(sizeof(char)*msg->data.len);
        		if(msg->data.buf == NULL){
        			errno = ENOMEM;
        			return -1;
        		}
				total = msg->data.len;
      			/* controllo di aver letto tutto il buffer */
				while((temp = read(fd, (msg->data.buf)+temp, total)) < total){
					if(temp == -1){
						errno = EBADE;
                    	return -1;
					}
					total -= temp;
				}
				return 0;
			}
	errno = EBADE;
	return -1;
}

void init_data(membox_key_t *key, char *data, unsigned int len);
int check_data(membox_key_t *key, char *data, unsigned int len);
void* threadFun(void* param);




int main(){
	int     			fd_skt;	// socket del server 
	int 		  		fd_c;	// socket del client
	int 				i;
	pthread_t 		  	threadServer;
	message_t*			messaggio;
	message_t*			messaggio1;
	struct sockaddr_un 	sa;
	
	//crea la comunicazione: socket
	strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
	sa.sun_family=AF_UNIX;
	
	fd_skt=socket(AF_UNIX,SOCK_STREAM,0);
	
	pthread_create(&threadServer, NULL, threadFun, NULL);
	
	messaggio = malloc(sizeof(message_t));
	messaggio1 = malloc(sizeof(message_t));
	
	sleep(1);
	// mi connetto al server
	connect(fd_skt,(struct sockaddr*) &sa, sizeof(sa));
	printf("MI SONO CONNESSO AL SERVER!\n");
	// creo N messaggi, li invio, aspetto le risposte e le controllo
	for (i=0; i< NUM; i++){
		//sleep(1);
		// crea messaggio
		messaggio->hdr.op = PUT_OP; //tanto hai incluso ops.h --- si ma tanto non mi serve, controlla solo data alla fine. comunque vabbè
		messaggio->hdr.key = i;
		messaggio->data.len = i*2000;
		messaggio->data.buf = malloc(sizeof(char)*messaggio->data.len);
		init_data(&(messaggio->hdr.key), messaggio->data.buf, messaggio->data.len); 
		if(check_data(&(messaggio->hdr.key), messaggio->data.buf, messaggio->data.len)==0) //non fai più il figo? --- sennò restituisce -1, non si può xD -- ah ecco xD
			printf("MESSAGGIO %d RICEVUTO CORRETTAMENTE!\n", i);
		else
			printf("MESSAGGIO %d ERRATO!!!!!!\n", i);

		// spedisci messaggio --- usiamo subito le nostre (i messaggi sono lunghi) -- includiamo connections.h o le copiamo qua sotto? --- boh, è uguale -- aggiunto in cima: se ci dà problemi, si copia
		sendRequest(fd_skt, messaggio);
		free(messaggio->data.buf);
		// leggi risposta
		readReply(fd_skt, messaggio);
		// stampa esito
		if(check_data(&(messaggio->hdr.key), messaggio->data.buf, messaggio->data.len)==0) //non fai più il figo? --- sennò restituisce -1, non si può xD -- ah ecco xD
			printf("MESSAGGIO %d RICEVUTO CORRETTAMENTE!\n", i);
		else
			printf("MESSAGGIO %d ERRATO!!!!!!\n", i);
		free(messaggio->data.buf);
	}
	close(fd_skt);	
	return 0;
}
// funzione server
/*
void* threadFun(void* param){
		int fd_skt, fd_c;
		int i;
		struct sockaddr_un sa;
	
	strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
	sa.sun_family=AF_UNIX;
	
	fd_skt=socket(AF_UNIX,SOCK_STREAM,0);
	bind(fd_skt,(struct sockaddr*) &sa, sizeof(sa));
	listen(fd_skt, SOMAXCONN);
	fd_c=accept(fd_skt,NULL,0);
	
		message_t* messaggio = malloc(sizeof(message_t));
	while(readReply(fd_c, messaggio)){
		sendRequest(fd_c, messaggio);
			free(messaggio->data.buf);
	}
}*/

// funzione server
void* threadFun(void* param){
		int fd_skt, fd_c;
		int i;
		struct sockaddr_un 		sa;
	struct sockaddr_un		client;
	struct sockaddr*		client_ptr;
	socklen_t 				client_len;
	
	strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
	sa.sun_family=AF_UNIX;
	
	fd_skt=socket(AF_UNIX,SOCK_STREAM,0);
	bind(fd_skt,(struct sockaddr*) &sa, sizeof(sa));
	listen(fd_skt, SOMAXCONN);
	client_ptr = (struct sockaddr*) &client;
	fd_c=accept(fd_skt, (struct sockaddr *) &client, &client_len);
	
	message_t* messaggio = malloc(sizeof(message_t));
	while(readReply(fd_c, messaggio))
	sendRequest(fd_c, messaggio);
}


void init_data(membox_key_t *key, char *data, unsigned int len) {
		int i;
		long *p = (long*)data;
		for(i=0;i<(len/sizeof(unsigned long));++i) setKey((membox_key_t*)&p[i], key);
		int r = len % sizeof(unsigned long);
		for(i=1;i<=r;++i) data[len-i] = 0x01;
}

int check_data(membox_key_t *key, char *data, unsigned int len) {
		int i;
		long *p = (long*)data;
		for(i=0;i<(len/sizeof(unsigned long));++i)
			if (p[i] != *key) {
				//printf("CLIENT: INVALID DATA: len = %d\nHO RICEVUTO %li, MA DOVEVO AVERE %li\n", len, *key, p[i]);
				return -1;
			 }
			 //else printf("%d UGUALE!!!!!\n",len);
		int r = len % sizeof(unsigned long);
		//printf("CLIENT: LEN: %d\nDATA OK!\n", len);
		for(i=1;i<=r;++i) 
		if (data[len-i] != 0x01) return -1;
		return 0;
}