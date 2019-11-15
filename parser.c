#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#define MAX_LEN 100
#define CONF_N 7


conf* parseConfig(FILE* file){
	char 	line[MAX_LEN];
	char**	strings;
	conf* 	params;
	int 	i;
	int 	k=0;


	strings=(char**)malloc(CONF_N*sizeof(char*));
	if(strings == NULL){
		return NULL;
	}
	while (fgets(line, sizeof(line), file)){
		i=0;
		if(line[i]!='#' && line[i]!='\n' && k<CONF_N){
			/* scorro la riga fino a raggiungere il carattere '=', ignorando poi lo spazio successivo */
			while(line[i]!='=')
				i++;
			i+=2;

			/* salvo tutti i caratteri della riga letta, a partire dall'i-esimo, in una stringa temporanea */
			char* temp = malloc(sizeof(char)*MAX_LEN);
			if(temp == NULL){
				free(strings);
				return NULL;
			}
			
			strcpy(temp, line+i);
			i=0;
			/* scorro fino ad incontrare uno spazio, un eof o un eol */
			while( (temp[i]!=' ') && (temp[i]!='\0') && (temp[i]!='\n') )
				i++;

			strings[k] = (char*) calloc((i+1), sizeof(char));
			/* copio i primi i caratteri di temp in strings[k] */
			strncpy(strings[k], temp, i);
			k++;
			free(temp);
		}
	}
	params = malloc(sizeof(conf));
	if(params==NULL){
		free(strings);
		return NULL;
	}
	params->UnixPath = malloc(sizeof(char)*MAX_LEN);
	if(params->UnixPath==NULL){
		free(strings);
		free(params);
		return NULL;
	}
	params->StatFileName = malloc(sizeof(char)*MAX_LEN);
	if(params->StatFileName==NULL){
		free(strings);
		free(params->UnixPath);
		free(params);
		return NULL;
	}
  
	/* assegno i valori letti ai parametri corrispondenti */
	strcpy (params->UnixPath, strings[0]);
	params->MaxConnections = atoi(strings[1]);
	params->ThreadsInPool = atoi(strings[2]);
	params->StorageSize = atoi(strings[3]);
	params->StorageByteSize = atoi(strings[4]);
	params->MaxObjSize = atoi(strings[5]);
	strcpy (params->StatFileName, strings[6]);

	/* dealloco le stringhe lette */
	for (i=0; i<CONF_N; i++)
		free(strings[i]);
	free (strings);

	return params;
}
