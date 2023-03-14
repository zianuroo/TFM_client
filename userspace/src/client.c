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

#define MAX 128
#define ADCDATACOLUMN 8
#define ADCDATAROW 201
#define PORT 8080
#define SA struct sockaddr

#define SERVER_ADDRESS "192.168.137.212"

char *CMD_CLIENT[] = {"RDDATA", "exit", "START", "END"};
char *CMD_SERVER[] = {"SNDATA", "exit", "ERR", "END"};

int16_t **data_adc_client = NULL; 


uint8_t number_chan(uint8_t enchan);
uint8_t chan_no(uint8_t enchan);
int client_TCP();
int client_UDP();
void dataSave(uint8_t enchan_adc_client, uint8_t nchan_adc_client, int16_t M_client, uint16_t ndata_adc_client);

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

int client_TCP()
{
	int sockfd;
	struct sockaddr_in servaddr;

	// socket create and verification
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		printf("socket creation failed...\n");
		exit(1);
	}
	else
		printf("Socket successfully created..\n");
	bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
	servaddr.sin_port = htons(PORT);

	// connect the client socket to server socket
	if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr))
		!= 0) {
		printf("connection with the server failed...\n");
		exit(1);
	}
	else
		printf("connected to the server..\n");


	// function for chat
	char buff[MAX];
	int n;
	int readdata=0;
	int iteration = 1;
	int received_block = 0;
	char delim[] = " ";
	
	uint16_t nblock_adc_client; 	// Define el tamaño del bloque de datos de un canal
	uint8_t nchan_adc_client;	// Define el numero de canales activados [0 - 8]
	int16_t M_client;		// Define cuantas transmisiones habra
	uint16_t ndata_adc_client;	// Tamaño de bloque de datos * numero de canales activados
	uint8_t enchan_adc_client;	// Define que canales son los que estan activados [0 - 255]
	
	for (;;) {
		
		// ESCRITURA DE COMANDO
		// Si estamos en la primera iteracion --> Se manda RDDATA
		if (iteration == 1) {
			bzero(buff, MAX);
			strncpy(buff, CMD_CLIENT[0], sizeof(buff));
			printf("To Server : %s \n", buff);
			write(sockfd, buff, sizeof(buff));
			iteration++;
		}
		
		// LECTURA DE RESPUESTA
		bzero(buff, sizeof(buff));
		// Si no estamos en un stream de datos, se lee el comando
		if(readdata == 0){
			read(sockfd, buff, sizeof(buff));
			//printf("From Server : %s \n", buff);
		}
		// Si estamos en un stream de datos, se lee el bloque entero de datos.
		else{
			ssize_t bytes_read = read(sockfd, &data_adc_client[received_block][0], sizeof(uint16_t)*(ndata_adc_client + 4));
			//printf("bytes read: %ld\n", bytes_read);
			//printf("sizeof int: %ld\n", sizeof(uint16_t));
			//printf("tranmsission byte size: %ld\n", bytes_read*sizeof(uint16_t));
			if (bytes_read == -1) {
				printf("read error \n"); // handle error
			}
			if ((size_t)bytes_read < sizeof(uint16_t)*(ndata_adc_client + 4)) {
				printf("not all the data was read, you may need to read the remaining data again\n");
			}

			// Impresion de valores recibidos
			//printValues(received_block, ndata_adc_client);

			received_block++;
			// Cuando ya se han recibido todos los bloques se vuelve a enviar START. El server respondera con END.
			if(received_block == M_client){
				readdata = 0;
				received_block = 0;
				bzero(buff, MAX);
				strncpy(buff, CMD_CLIENT[2], sizeof(buff));
				write(sockfd, buff, sizeof(buff));
			}
			else{
				bzero(buff, MAX);
				strncpy(buff, CMD_CLIENT[2], sizeof(buff));
				write(sockfd, buff, sizeof(buff));
			}
		}
		
		// Si la respuesta es SNDATA --> Se manda START
		if ((strncmp(buff, CMD_SERVER[0], 6)) == 0) {
			char *ptr = strtok(buff, delim);
			int infonum = 0;
			while(ptr != NULL){
				char aux_str[80];
				sprintf(aux_str,"%s", ptr);
				//printf("Info%d: %s\n", infonum, aux_str);
				ptr = strtok(NULL, delim);
				if(infonum == 1){
					enchan_adc_client = atoi(aux_str);
					nchan_adc_client = number_chan(enchan_adc_client);
					printf("	Canales activados en el sistema: %d\n", nchan_adc_client);
					n = 0;
					printf("	| ");
					for (int j=0;j<nchan_adc_client;j++) {
						n += chan_no(enchan_adc_client>>n);
						printf("AI%d | ", n);
					}
					printf("\n");
				}
				else if(infonum == 2){
					nblock_adc_client = atoi(aux_str);
					printf("	NBLOCK ADC: %d\n", nblock_adc_client);
				}
				else if(infonum == 3){
					M_client = atoi(aux_str);
					printf("	M: %d\n", M_client);
				}
				infonum++;
			}
			// Una vez se sabe el tamaño de la transmision, se puede asignar memoria dinamica para guardar la cantidad de datos necesaria
			ndata_adc_client = nblock_adc_client * nchan_adc_client;
			printf("	Tamaño ndata: %d\n", ndata_adc_client);
			/* allocate rows */
			if((data_adc_client = malloc(M_client * sizeof(int16_t *))) == NULL){
				printf("Failed to allocate rows");
				free(data_adc_client);
				data_adc_client = NULL;
			}
			else{
				//printf("Memoria asignada a los punteros de los bloques\n");
				/* allocate cols */
				int c;
				for(c = 0; c < M_client; c++){
					if((data_adc_client[c] = (int16_t *) calloc((ndata_adc_client+4),sizeof(int16_t))) == NULL){
						printf("Failed to allocate cols");
						for (int l=0; l<c; l++) {
							free(data_adc_client[l]);
							data_adc_client[l]=NULL;
						}
					}
					else{
						//printf("Memoria asignada al bloque %d\n", c);
					}
				}
			
			}
			//printf("Comienzo de stream de datos...\n");
		
		
		bzero(buff, MAX);
		strncpy(buff, CMD_CLIENT[2], sizeof(buff));
		write(sockfd, buff, sizeof(buff));
		readdata = 1;
		
		}
		
		// Respuesta END
		else if ((strncmp(buff, CMD_SERVER[3], 3)) == 0) {
			bzero(buff, MAX);
			strncpy(buff, CMD_CLIENT[3], sizeof(buff));
			write(sockfd, buff, sizeof(buff));
			//printf("END received...\n");
			readdata = 0;
		}
		
		// Respuesta exit
		else if ((strncmp(buff, CMD_SERVER[1], 4)) == 0) {
			bzero(buff, MAX);
			//printf("Client Exit...\n");
			break;
		}
		
		//printf("Comando enviado : %s \n", buff);
		
	}
	
	// Transmision de datos acabada se reconstruye la señal para que sea procesable a la hora de guardarlo en un txt
	dataSave(enchan_adc_client, nchan_adc_client, M_client, ndata_adc_client);
	
	/* free cols */
	int i;
	for (i=0; i<M_client; i++) {
		//printf ("free(data_adc_client[%d]);\n", i);
		free(data_adc_client[i]);
		data_adc_client[i]=NULL;
	}
	/* free rows */
	free(data_adc_client);
	data_adc_client = NULL;

	// close the socket
	close(sockfd);

	return 0;
}


int client_UDP()
{
	int sockfd;
	struct sockaddr_in servaddr;
	int len = sizeof(servaddr);

	// socket create and verification
	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  // UDP: SOCK_DGRAM
	if (sockfd == -1) {
		printf("socket creation failed...\n");
		exit(1);
	}
	else
		printf("Socket successfully created..\n");
	
	bzero(&servaddr, sizeof(servaddr));
	
	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
	servaddr.sin_port = htons(PORT);
	
	
	char buff[MAX];
	int n;
	int readdata=0;
	int iteration = 1;
	int received_block = 0;
	char delim[] = " ";
	
	uint16_t nblock_adc_client; 	// Define el tamaño del bloque de datos de un canal
	uint8_t nchan_adc_client;	// Define el numero de canales activados [0 - 8]
	int16_t M_client;		// Define cuantas transmisiones habra
	uint16_t ndata_adc_client;	// Tamaño de bloque de datos * numero de canales activados
	uint8_t enchan_adc_client;	// Define que canales son los que estan activados [0 - 255]
	
	for (;;) {
		
		// ESCRITURA DE COMANDO
		// Si estamos en la primera iteracion --> Se manda RDDATA
		if (iteration == 1) {
			bzero(buff, MAX);
			strncpy(buff, CMD_CLIENT[0], sizeof(buff));
			// Send the message to server:
			if(sendto(sockfd, buff, sizeof(buff), 0, (SA*)&servaddr, len) < 0){
				printf("Unable to send message\n");
				
			}
			else{
				//printf("Command sent: %s\n", buff);
			}
			iteration++;
		}
		
		// LECTURA DE RESPUESTA
		bzero(buff, sizeof(buff));
		// Si no estamos en un stream de datos, se lee el comando
		if(readdata == 0){
		    	// Receive the server's response:
			if(recvfrom(sockfd, buff, sizeof(buff), 0, (SA*)&servaddr, (socklen_t *)&len) < 0){
				printf("Error while receiving server's msg\n");
				
			}
			//printf("From Server : %s \n", buff);
		}
		// Si estamos en un stream de datos, se lee el bloque entero de datos.
		else{
		
		    	// Receive the server's response:
			if(recvfrom(sockfd, &data_adc_client[received_block][0], sizeof(uint16_t)*(ndata_adc_client + 4), 0, (SA*)&servaddr, (socklen_t *)&len) < 0){
				printf("Error while receiving server's msg\n");
				
			}
			
			// Impresion de valores recibidos
			//printValues(received_block, ndata_adc_client);

			received_block++;
			// Cuando ya se han recibido todos los bloques se vuelve a enviar START. El server respondera con END.
			if(received_block == M_client){
				readdata = 0;
				received_block = 0;
				bzero(buff, MAX);
				strncpy(buff, CMD_CLIENT[2], sizeof(buff));
				// Send the message to server:
				if(sendto(sockfd, buff, sizeof(buff), 0, (SA*)&servaddr, len) < 0){
					printf("Unable to send message\n");
					
				}
				else{
					//printf("Command sent: %s\n", buff);
				}
			}
			else{
				bzero(buff, MAX);
				strncpy(buff, CMD_CLIENT[2], sizeof(buff));
				// Send the message to server:
				if(sendto(sockfd, buff, sizeof(buff), 0, (SA*)&servaddr, len) < 0){
					printf("Unable to send message\n");
					
				}
				else{
					//printf("Command sent: %s\n", buff);
				}
			}
		}
		
		// Si la respuesta es SNDATA --> Se manda START
		if ((strncmp(buff, CMD_SERVER[0], 6)) == 0) {
			char *ptr = strtok(buff, delim);
			int infonum = 0;
			while(ptr != NULL){
				char aux_str[80];
				sprintf(aux_str,"%s", ptr);
				//printf("Info%d: %s\n", infonum, aux_str);
				ptr = strtok(NULL, delim);
				if(infonum == 1){
					enchan_adc_client = atoi(aux_str);
					nchan_adc_client = number_chan(enchan_adc_client);
					printf("	Canales activados en el sistema: %d\n", nchan_adc_client);
					n = 0;
					printf("	| ");
					for (int j=0;j<nchan_adc_client;j++) {
						n += chan_no(enchan_adc_client>>n);
						printf("AI%d | ", n);
					}
					printf("\n");
				}
				else if(infonum == 2){
					nblock_adc_client = atoi(aux_str);
					printf("	NBLOCK ADC: %d\n", nblock_adc_client);
				}
				else if(infonum == 3){
					M_client = atoi(aux_str);
					printf("	M: %d\n", M_client);
				}
				infonum++;
			}
			// Una vez se sabe el tamaño de la transmision, se puede asignar memoria dinamica para guardar la cantidad de datos necesaria
			ndata_adc_client = nblock_adc_client * nchan_adc_client;
			printf("	Tamaño ndata: %d\n", ndata_adc_client);
			/* allocate rows */
			if((data_adc_client = malloc(M_client * sizeof(int16_t *))) == NULL){
				printf("Failed to allocate rows");
				free(data_adc_client);
				data_adc_client = NULL;
			}
			else{
				//printf("Memoria asignada a los punteros de los bloques\n");
				/* allocate cols */
				int c;
				for(c = 0; c < M_client; c++){
					if((data_adc_client[c] = (int16_t *) calloc((ndata_adc_client+4),sizeof(int16_t))) == NULL){
						printf("Failed to allocate cols");
						for (int l=0; l<c; l++) {
							free(data_adc_client[l]);
							data_adc_client[l]=NULL;
						}
					}
					else{
						//printf("Memoria asignada al bloque %d\n", c);
					}
				}
			
			}
			//printf("Comienzo de stream de datos...\n");
		
		
		bzero(buff, MAX);
		strncpy(buff, CMD_CLIENT[2], sizeof(buff));
		// Send the message to server:
		if(sendto(sockfd, buff, sizeof(buff), 0, (SA*)&servaddr, len) < 0){
			printf("Unable to send message\n");
			
		}
		else{
			//printf("Command sent: %s\n", buff);
		}
		readdata = 1;
		
		}
		
		// Respuesta END
		else if ((strncmp(buff, CMD_SERVER[3], 3)) == 0) {
			bzero(buff, MAX);
			strncpy(buff, CMD_CLIENT[3], sizeof(buff));
			// Send the message to server:
			if(sendto(sockfd, buff, sizeof(buff), 0, (SA*)&servaddr, len) < 0){
				printf("Unable to send message\n");
				
			}
			else{
				//printf("Command sent: %s\n", buff);
			}
			//printf("END received...\n");
			readdata = 0;
		}
		
		// Respuesta exit
		else if ((strncmp(buff, CMD_SERVER[1], 4)) == 0) {
			bzero(buff, MAX);
			//printf("Client Exit...\n");
			break;
		}
		
		//printf("Comando enviado : %s \n", buff);
		
	}
	
	// Transmision de datos acabada se reconstruye la señal para que sea procesable a la hora de guardarlo en un txt
	dataSave(enchan_adc_client, nchan_adc_client, M_client, ndata_adc_client);
	
	/* free cols */
	int i;
	for (i=0; i<M_client; i++) {
		//printf ("free(data_adc_client[%d]);\n", i);
		free(data_adc_client[i]);
		data_adc_client[i]=NULL;
	}
	/* free rows */
	free(data_adc_client);
	data_adc_client = NULL;

	// close the socket
	close(sockfd);

	return 0;
}

void dataSave(uint8_t enchan_adc_client, uint8_t nchan_adc_client, int16_t M_client, uint16_t ndata_adc_client){
	// Transmision de datos acabada se reconstruye la señal para que sea procesable a la hora de guardarlo en un txt
	char *readingsDir = 	"./ADCreadings";
	char aux[100];
	int n = 0;
	// Create the directory
	if (mkdir(readingsDir, 0700) == -1) {
		if (errno != EEXIST) {
			perror("Error creating directory");
		}
	}
	
	
	
	//printf("Proceeding to save the results...\n");
	for (int j=0;j<nchan_adc_client;j++) {
		n += chan_no(enchan_adc_client>>n);
		bzero(aux, 100);
		sprintf(aux, "./ADCreadings/AI%dlec.txt", n);		
		FILE *fp = fopen(aux, "w");
		if (fp == NULL){
			printf("Error opening the file %s", aux);
		}
		else{
			for (int i=0;i<M_client;i++) {
				for (int k=0;k<ndata_adc_client/nchan_adc_client;k++) {
					fprintf(fp, "%d\n", data_adc_client[i][(j+3)+nchan_adc_client*k]);
				}
			}
		}
		fclose(fp);
		//printf("	AI%d Succesfully saved!\n",n);
	}
	
	int nerr=0;
	n = 0;
	bzero(aux, 100);
	sprintf(aux, "./ADCreadings/log.txt");		
	FILE *fp = fopen(aux, "w");
	for (int j=0;j<nchan_adc_client;j++) {
		n += chan_no(enchan_adc_client>>n);
		fprintf(fp,"For channel %d\n", n);
		for (int i=0;i<M_client;i++) {
			for (int k=0;k<ndata_adc_client/nchan_adc_client;k++) {
				// printf("Chan%d[%d] = %d\n",n,i*ndata_adc_client/nchan_adc_client+k,*(data_adc+(ndata_adc_client+4)*i+(j+3)+nchan_adc_client*k));
				if (adc_data[(i*ndata_adc_client/nchan_adc_client+k)%201][n-1] != data_adc_client[i][(j+3)+nchan_adc_client*k]) { //[j*(ndata_adc_client/nchan_adc_client)+k]
					fprintf(fp,"Error in [%d] --> Expected: %d // Received %d\n", 
					i*ndata_adc_client/nchan_adc_client+k, 
					adc_data[(i*ndata_adc_client/nchan_adc_client+k)%201][n-1], 
					data_adc_client[i][(j+3)+nchan_adc_client*k]);
					
					nerr++;
				}
			}
		}
		if(nerr == 0){
			fprintf(fp,"	No errors!\n");
		}
		fprintf(fp,"\n\n");
		nerr = 0;
	}
	printf("	Log and data succesfully saved!\n");
	

}

void printValues(int received_block, uint16_t ndata_adc_client){

			printf("	Fin bloque de datos %d\n", received_block);
			//Impresion de valores recibidos
			for(int i=0; i<(ndata_adc_client + 4); i++){
				if ((i == 0) || (i == 1) || (i == 2)){
					printf("	CMD ID: 0x%08x\n", data_adc_client[received_block][i]);
				}
				else if( i == ((ndata_adc_client + 4)-1)){
					printf("	CRC16: 0x%08x\n", data_adc_client[received_block][i]);
				}
				else{
					//printf("	%d \n", data_adc_client[received_block][i]);
				}				
			}
			


}


uint8_t number_chan(uint8_t enchan) {

	uint8_t n = 0;

	while (enchan!=0) {
		n += (enchan & 0x01);
		enchan >>= 1;
	}

	return n;
}


uint8_t chan_no(uint8_t enchan) {
	int n=1;
	while ((enchan & 0x01)==0) {
		n++;
		enchan >>= 1;
	}
	return n;
}

