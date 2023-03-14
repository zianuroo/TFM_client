
#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <math.h>
#include <stdint.h>
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

int client_TCP()
{
	int sockfd;
	struct sockaddr_in servaddr;
	char buff[MAX];
	int n;
	int readdata=0;
	int iteration = 1;
	int received_block = 0;
	char delim[] = " ";
	int demo = 0;
	
	uint16_t nblock_adc_client; 	// Define el tamaño del bloque de datos de un canal
	uint8_t nchan_adc_client;	// Define el numero de canales activados [0 - 8]
	int16_t M_client;		// Define cuantas transmisiones habra
	uint16_t ndata_adc_client;	// Tamaño de bloque de datos * numero de canales activados
	uint8_t enchan_adc_client;	// Define que canales son los que estan activados [0 - 255]

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
			if (bytes_read == -1) {
				printf("read error \n"); // handle error
			}
			if ((size_t)bytes_read < sizeof(uint16_t)*(ndata_adc_client + 4)) {
				printf("not all the data was read, you may need to read the remaining data again\n");
			}

			// Impresion de valores recibidos
			printDemoValues(nchan_adc_client, received_block, ndata_adc_client);

			received_block++;
			// Cuando ya se han recibido todos los bloques se vuelve a enviar START. El server respondera con END.
			if(demo == 0){
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
			// Si se va a estar recibiendo datos de manera continua
			else{
				// Como a received block se le ha hecho una suma, si ahora vale 2, es que acabamos de escribir en &data_adc_client[1][0]
				// y la siguiente iteracion debera ser escrita en &data_adc_client[0][0]
				if (received_block == 2){ 
					received_block = 0;
				}
				else{
					received_block = 1;
				}
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
					if (M_client == -1){
						printf("		Transmision de datos continua\n");
						demo = 1;
					}
				}
				infonum++;
			}
			// Una vez se sabe el tamaño de la transmision, se puede asignar memoria dinamica para guardar la cantidad de datos necesaria
			ndata_adc_client = nblock_adc_client * nchan_adc_client;
			printf("	Tamaño ndata: %d\n", ndata_adc_client);
			printf("Valores recibidos\n");
			printf("	| ");
			n = 0;
			for (int j=0;j<nchan_adc_client;j++) {
				n += chan_no(enchan_adc_client>>n);
				printf("  AI%d  | ", n);
			}
			printf("\n");

			if (demo == 1){
				M_client = 2;
			}
			
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