#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>


#define PORT 12000
#define MAXLINE 1024

typedef struct pacote{
	// Número sequencial
	int sequencialNum;	
	// Checksum			
	long int checksum;
	// Informações (1024 bytes)	
	char data[MAXLINE];
	// Tamanho da string
	int sizeData;				
}pacote;

typedef struct ack{
	// 1 indica sucesso e 0 indica falha
	int status;
}ack;

void searchArchive(char *buf, char *name, char *clientIp);
void updateArchive(char *name, char *newIp);

int main(int argc, char *argv[]) {
	int socketfd, check;

	struct sockaddr_in serverAddr, clientAddr;
	unsigned int serverLen, clientLen;

	pacote pkt;
	ack reply;

	char buffer[MAXLINE], answer[MAXLINE];

	//=====================================CONFIGURACAO=========================================
	// Preenchendo informações sobre o servidor
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(PORT);
	serverLen = sizeof(serverAddr);

	if ( (socketfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("Erro na criação do socket!");
		exit(1);
	}

	if (bind(socketfd, (struct sockaddr *)&serverAddr, serverLen) < 0 ){
		perror("Erro no bind!");
		exit(1);
	}
	//========================================================================================


	printf("Tudo configurado, aguardando conexoes...\n");

	while(1) {
		memset(&clientAddr, 0, sizeof(clientAddr));
		clientLen = sizeof(clientAddr);

		// Recebendo dados
		check = recvfrom(socketfd, &pkt, sizeof(pkt), 0, (struct sockaddr *) &clientAddr, &clientLen);
		
		printf("Recebi de %s:UDP%u : %s \n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), pkt.data);

		// Operação de retornar o IP do detentor do arquivo
		if(pkt.data[0] == '>'){			
			searchArchive(answer, pkt.data, inet_ntoa(clientAddr.sin_addr));
			strcpy(pkt.data, answer);
			sendto(socketfd, &pkt, sizeof(pkt), 0, (struct sockaddr *) &clientAddr, clientLen);
		}
		// Operação de atualizar o BD
		else if(pkt.data[0] == '!'){
			pkt.data[0] = '>';
			updateArchive(pkt.data, inet_ntoa(clientAddr.sin_addr));
		}
		
	}

	return 0;
}

void searchArchive(char *buf, char *name, char *clientIp){
	FILE *file;
	file = fopen("bd.txt", "r");
	char arq[MAXLINE], ip[MAXLINE];

	while(fscanf(file, "%s %s", arq, ip) != EOF) {
		if(!strcmp(arq, name) && strcmp(ip, clientIp)) {
			fclose(file);
			strcpy(buf, ip);
			return;
		}
	}

	strcpy(buf, "!");
	fclose(file);
}

void updateArchive(char *name, char *newIp){	
	FILE *file;	
	file = fopen("bd.txt", "r");
	char arq[MAXLINE], ip[MAXLINE];
	while(fscanf(file, "%s %s", arq, ip) != EOF){
		if(!strcmp(arq, name) && !strcmp(ip, newIp)) return;
	}
	fclose(file);

	file = fopen("bd.txt", "a");
	fprintf(file, "%s %s\n", name, newIp);
	fclose(file);
}
