#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // bzero()
#include <sys/socket.h>
#include <unistd.h> // read(), write(), close()
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "../include/dataBank.h"
#include "../include/client.h"
#include "../include/client_funcs.h"
#include "../include/client_TCP.h"
#include "../include/client_UDP.h"

char *CMD_CLIENT[] = {"RDDATA", "exit", "START", "END"};
char *CMD_SERVER[] = {"SNDATA", "exit", "ERR", "END"};

int16_t **data_adc_client = NULL; 


int main(int argc, char *argv[]){
	int protocolo = 0;
	int end = 1;
	
	for (int i=1;i<argc;i++) {
		if (strcmp(argv[i],"-UDP")==0) {
			protocolo = 1;
		}
	}
	
	if (protocolo == 0){
		printf("Protocolo: TCP\n");
		end = client_TCP();
	}
	else{
		printf("Protocolo: UDP\n");	
		end = client_UDP();
	}
	
	if (end  == 0){
		printf("Data transmission finished\n");
	}
	else{
		printf("Data transmission failed\n");
	}
	
}