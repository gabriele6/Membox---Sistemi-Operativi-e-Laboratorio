/*
 * membox Progetto del corso di LSO 2016 
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Pelagatti, Torquati
 * 
 */
#ifndef CONNECTIONS_H_
#define CONNECTIONS_H_

#define MAX_RETRIES     10
#define MAX_SLEEPING     3
#if !defined(UNIX_PATH_MAX)
#define UNIX_PATH_MAX  64
#endif

#include <message.h>

/**
 * @file  connections.h
 * @brief Contiene le funzioni che implementano il protocollo 
 *        tra i clients ed il server membox
 */

/**
 * @function openConnection
 * @brief Apre una connessione AF_UNIX verso il server membox.
 *
 * @param path Path del socket AF_UNIX 
 * @param ntimes numero massimo di tentativi di retry
 * @param secs tempo di attesa tra due retry consecutive
 *
 * @return il descrittore associato alla connessione in caso di successo
 * @return -1 in caso di errore
 *
 * @exception ECONNABORTED fallita creazione della socket 
 * @exception ECONNREFUSED fallita la connessione alla socket del server
 */
int openConnection(char* path, unsigned int ntimes, unsigned int secs);

// -------- server side ----- 

/**
 * @function readHeader
 * @brief Legge l'header del messaggio
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da ricevere
 *
 * @return 	0 in caso di successo 
 * @return -1 in caso di errore
 *
 * @exception EBADE fallita la lettura dell'header 
 */
 int readHeader(long fd, message_hdr_t *hdr);

/**
 * @function readData
 * @brief Legge il body del messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al body del messaggio
 *
 * @return 0 in caso di successo 
 * @return -1 in caso di errore
 *
 * @exception EBADE fallita la lettura del dato
 */
int readData(long fd, message_data_t *data);


/* da completare da parte dello studente con altri metodi di interfaccia */



// ------- client side ------
/**
 * @function sendRequest
 * @brief Invia un messaggio di richiesta al server membox
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return 0 in caso di successo 
 * @return -1 in caso di errore
 *
 * @exception ECOMM fallita la scrittura di un messaggio
 */
int sendRequest(long fd, message_t *msg);


/**
 * @function sendHeader
 * @brief Invia un header 
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header da inviare
 *
 * @return 0 in caso di successo 
 * @return -1 in caso di errore
 *
 * @exception ECOMM fallita la scrittura dell'header
 */
int sendHeader(long fd, message_hdr_t *hdr);

/**
 * @function readReply
 * @brief Legge un messaggio di risposta dal server membox
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da ricevere
 *
 * @return 0 in caso di successo 
 * @return -1 in caso di errore
 *
 * @exception EBADE fallita la lettura del messaggio
 * @exception ENOMEM fallita l'allocazione del buffer
 */
int readReply(long fd, message_t *msg);

/**
 * @function readRequest
 * @brief Legge un messaggio di richiesta dal client
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da ricevere
 *
 * @return 0 in caso di successo 
 * @return -1 in caso di errore
 *
 * @exception EBADE fallita la lettura del messaggio
 * @exception ENOMEM fallita l'allocazione del buffer
 */
int readRequest(long fd, message_t *msg);


#endif /* CONNECTIONS_H_ */
