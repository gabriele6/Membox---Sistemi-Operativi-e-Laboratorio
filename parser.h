#ifndef PARSER
#define PARSER
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_LEN 100
#define CONF_N 7

/**
 * @file  parser.h
 * @brief Contiene la struttura dati implementativa del parser del file di configurazione
 */

/* -------------------- Struttura Dati -------------------- */
 /**
 *  @struct conf
 *  @brief parametri di configurazione
 *
 */
typedef struct{
	char*	UnixPath;			///< path della socket AF_UNIX
	int 	MaxConnections;		///< numero massimo di connessioni pendenti
	int 	ThreadsInPool;		///< numero di thread worker nel pool
	int 	StorageSize;		///< numero massimo di oggetti nella repository
	int 	StorageByteSize;	///< dimensione della repository in bytes
	int 	MaxObjSize;			///< massima dimensione di un oggetto della repository
	char*	StatFileName;		///< path del file delle statistiche
} conf;


/* -------------------- parseConfig -------------------- */
/**
 * @function parseConfig
 * @brief Legge il file di configurazione, salvando i parametri in una struttura dati
 *
 * @param file puntatore al file di configurazione
 *
 * @return la struttura dati contenente i parametri letti dal file di configurazione
 */
conf* parseConfig(FILE* file);

#endif
