/*
 * membox Progetto del corso di LSO 2016 
 *
 * 
 * 
 */
#include <connections.h>
#include <message.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>	
#include <errno.h>

#define N 100

int openConnection(char* path, unsigned int ntimes, unsigned int secs){
	int 				i;
	int 				ris;
	int 				fd;
	struct sockaddr_un 	sa;
	
	/* creo una socket per il client */
	strncpy(sa.sun_path, path, UNIX_PATH_MAX);
	sa.sun_family=AF_UNIX;
	fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (fd==-1){
		errno = ECONNABORTED;
		return -1;
    }
	
	/* prova a connettersi ntimes volte, a secs secondi di distanza */
	for (i=0; i<ntimes; i++){
		ris = connect(fd, (struct sockaddr*) &sa, sizeof(sa));
		if (ris!=-1)
			return fd;
    	if (i==ntimes-1){
        	errno = ECONNREFUSED;
			return -1;
        }
		sleep(secs);
	}
	errno = ECONNREFUSED;
	return -1;
}


int readHeader(long fd, message_hdr_t *hdr){
	if(read(fd, &(hdr->op), sizeof(op_t)) >= sizeof(op_t)){			
		if(read(fd, &(hdr->key), sizeof(membox_key_t)) >= sizeof(membox_key_t)){
			return 0;
		}
	}
	errno = EBADE;
	return -1;
}

int readData(long fd, message_data_t *data){
	int temp = 0;
	int total = 0;

	if(read(fd, &(data->len), sizeof(unsigned int))>=sizeof(unsigned int)){	
		data->buf = malloc(sizeof(char)*data->len);
		total = data->len;
		/* controllo di aver letto tutto il buffer */
		while((temp = read(fd, (data->buf)+temp, total)) < total){
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

int sendHeader(long fd, message_hdr_t *hdr){
	if(write(fd, &(hdr->op), sizeof(op_t)))
		if(write(fd, &(hdr->key), sizeof(membox_key_t)))
			return 0;
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

int readRequest(long fd, message_t *msg){
	int temp = 0;
	int total = 0;

	if(read(fd, &(msg->hdr.op), sizeof(op_t)) >= sizeof(op_t))
		if(read(fd, &(msg->hdr.key), sizeof(membox_key_t)) >= sizeof(membox_key_t))
			if(read(fd, &(msg->data.len), sizeof(unsigned int)) >= sizeof(unsigned int)){
       			msg->data.buf = malloc(sizeof(char)*msg->data.len);
       			if(msg->data.buf == NULL){
        			errno = ENOMEM;
        			return -1;
        		}
				total = msg->data.len;
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
