#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <algorithm>

#define PORT 12000
#define MAXLINE 1024

using namespace std;

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

long int checksumFunc(char data[MAXLINE]){
	long int sum = 0;

	for(int i = 0; i<strlen(data); i++){
		int aux = (int) data[i];
		sum += (aux * i);
	}

	return sum;
}

void prepararPacote(pacote *pacote, int num, char inf[MAXLINE], int sz){
	pacote->sequencialNum = num;
	pacote->checksum = checksumFunc(inf);
	strcpy(pacote->data, inf);
	pacote->sizeData = sz;
}

int main(int argc, char *argv[]) {
	// Variáveis utilizadas no programa
	int socketfd, check, opt, fileSize, pkt_count, erros;
	long long int checksum;

	FILE *fp;

	char nomeDoArquivo[MAXLINE], buffer[MAXLINE];
	pacote pkt; pkt.sizeData = 1;
	ack reply;

	struct sockaddr_in clientAddr, serverAddr, peerAddr;
	unsigned int clientLen, serverLen, peerLen;
	struct timeval temporizador;
	struct hostent *host;

	//=====================================CONFIGURACAO=========================================
	// Checa os argumentos passados
	if(argc != 2) {
		printf("Como usar : %s <server>\n", argv[0]);
		exit(1);
	}

	// Pega o ip do servidor
	host = gethostbyname(argv[1]);
	if(host == NULL) {
		printf("Host desconhecido ou invalido!\n");
		exit(1);
	}

	// Configuração do servidor
	serverAddr.sin_family = host->h_addrtype;
	memcpy((char *) &serverAddr.sin_addr.s_addr, host->h_addr_list[0], host->h_length);
	serverAddr.sin_port = htons(PORT);
	serverLen = sizeof(serverAddr);

	if ((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Erro na criação do socket!\n");
		exit(1);
	}

	// Configuração do cliente
	clientAddr.sin_family = AF_INET;
	clientAddr.sin_addr.s_addr = INADDR_ANY;
	clientAddr.sin_port = htons(PORT);
	clientLen = sizeof(clientAddr);


	if( (bind(socketfd, (struct sockaddr *) &clientAddr, clientLen)) < 0) {
		printf("Erro no binding da porta!\n");
		exit(1);
	}

	memset(&peerAddr, 0, sizeof(peerAddr));
	peerLen = sizeof(peerAddr);

	//========================================================================================

	printf("Digite uma opcao:\n");
	printf("1 - Download\n");
	printf("2 - Upload\n");
	printf("3 - Semear arquivos no servidor\n");
	printf("Digite a opcao: ");
	scanf("%d", &opt);

	switch(opt){

		// =========================== DOWNLOAD ==============================================
		case 1:
			printf("\nDigite o nome do arquivo: ");
			scanf("%s", nomeDoArquivo);
			strcpy(buffer, ">");
			strcat(buffer, nomeDoArquivo);
			prepararPacote(&pkt, 1, buffer, strlen(buffer));
			
			// Pedindo um IP para o rastreador
			check = sendto(socketfd, &pkt, sizeof(pkt), 0, (struct sockaddr *) &serverAddr, serverLen);
			check = recvfrom(socketfd, &pkt, sizeof(pkt), 0, (struct sockaddr *) &serverAddr, &serverLen);
			
			// O arquivo não existe
			if(strcmp(pkt.data, "!") == 0){
				printf("O arquivo nao contem detentores validos\n");
			}
			else{
				// O arquivo existe
				printf("Preenchendo informações sobre o portador do arquivo: %s\n", pkt.data);

				// Preenchendo informações sobre quem tem o arquivo
				host = gethostbyname(pkt.data);
				peerAddr.sin_family = host->h_addrtype;
				memcpy((char *) &peerAddr.sin_addr.s_addr, host->h_addr_list[0], host->h_length);
				peerAddr.sin_port = htons(PORT);
				peerLen = sizeof(peerAddr);

				// Enviando requisição com o nome do arquivo
				printf("Enviando requisicao de arquivo para %s:UDP%u\n", inet_ntoa(peerAddr.sin_addr), ntohs(peerAddr.sin_port));
				prepararPacote(&pkt, 1, nomeDoArquivo, strlen(nomeDoArquivo));
				check = sendto(socketfd, &pkt, sizeof(pkt), 0, (struct sockaddr *) &peerAddr, peerLen);
			

				// Criando um novo arquivo
				FILE *fp;
				fp = fopen(nomeDoArquivo, "wb");
				
					
				// Recebendo o arquivo do outro cliente
				printf("Recebendo o arquivo de %s:UDP%u\n", inet_ntoa(peerAddr.sin_addr), ntohs(peerAddr.sin_port));
				
				// Definição de temporizador e numero de pacote esperado, bem como quantidade
				// de erros 
				erros = 0;
				pkt_count = 1;
				temporizador.tv_sec = 0;
				temporizador.tv_usec = 500000;
				if((setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, (void *) &temporizador, sizeof(temporizador))) < 0){
					perror("Erro ao definir temporizador!\n");
					exit(1);
				}
				while(1){
					do{
						check = recvfrom(socketfd, &pkt, sizeof(pkt), 0, (struct sockaddr *) &peerAddr, &peerLen);
						if(check < 0) erros++;
						else erros = 0;
						// Se a quantidade de erros seguidos no recebimento for muito grande
						// o host provavelmente não está ativo
						if(erros == 5){
							printf("O host detentor do arquivo não parece estar ativo!\n");
							break;
						}

						// Se o numero do pacote falhar ou se o checksum estiver incorrento
						// ack = 0;
						if(pkt.sizeData && 
							(pkt.sequencialNum != pkt_count 
							|| pkt.checksum != checksumFunc(pkt.data))) reply.status = 0;
						else reply.status = 1;

						check = sendto(socketfd, &reply, sizeof(reply), 0, (struct sockaddr *) &peerAddr, peerLen);

					}while(pkt.sizeData && (pkt.sequencialNum != pkt_count || pkt.checksum != checksumFunc(pkt.data)));
					
					if(erros == 5) break;
					pkt_count++;
					if(!pkt.sizeData) break;
					fwrite(pkt.data, 1, pkt.sizeData, fp);
				}
				fclose(fp);

				if(erros == 5){
					printf("Erro no recebimento do arquivo\n");
					remove(nomeDoArquivo);
				}
				else{
					printf("Arquivo completamente recebido\n");

					// Avisando o server que agora contem o arquivo
					strcpy(buffer, "!");
					strcat(buffer, nomeDoArquivo);
					prepararPacote(&pkt, 1, buffer, strlen(buffer));
					check = sendto(socketfd, &pkt, sizeof(pkt), 0, (struct sockaddr *) &serverAddr, serverLen);
				}
			}	
			break;
		case 2:
			// =========================== UPLOAD ==============================================
			check = recvfrom(socketfd, &pkt, sizeof(pkt), 0, (struct sockaddr *) &peerAddr, &peerLen);
			printf("Requisao do arquivo %s vinda de %s:UDP%u\n", pkt.data, inet_ntoa(peerAddr.sin_addr), ntohs(peerAddr.sin_port));
			
			// Abrindo o arquivo para ler	
			fp = fopen(pkt.data, "rb");

			// Definição de temporizador e numero de pacote
			pkt_count = 1;
			temporizador.tv_sec = 0;
			temporizador.tv_usec = 500000;
			if((setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, (void *) &temporizador, sizeof(temporizador))) < 0){
				perror("Erro ao definir temporizador!\n");
				exit(1);
			}
			// Lendo o arquivo
			while((fileSize = fread(pkt.data, 1, sizeof(pkt.data), fp)) > 0){
				prepararPacote(&pkt, pkt_count++, pkt.data, fileSize);
				
				reply.status = 0;
				// Manda o pacote até receber um ack positivo
				do{
					check = sendto(socketfd, &pkt, sizeof(pkt), 0, (struct sockaddr *) &peerAddr, peerLen);
					check = recvfrom(socketfd, &reply, sizeof(reply), 0, (struct sockaddr *) &peerAddr, &peerLen);
				}while(reply.status == 0);
			}
			fclose(fp);

			// Acabou o arquivo
			pkt.sizeData = 0;

			reply.status = 0;
			// Manda o pacote até receber um ack positivo
			do{
				check = sendto(socketfd, &pkt, sizeof(pkt), 0, (struct sockaddr *) &peerAddr, peerLen);
				check = recvfrom(socketfd, &reply, sizeof(reply), 0, (struct sockaddr *) &peerAddr, &peerLen);
			}while(reply.status == 0);

			break;
		case 3:
			printf("Digite os nomes dos arquivos que quer adicionar (Digite '!' para finalizar): \n");
			while(1){
				
				scanf("%s", nomeDoArquivo);
				if(!strcmp(nomeDoArquivo, "!")) break;

				// Avisando o server que agora contem o arquivo
				strcpy(buffer, "!");
				strcat(buffer, nomeDoArquivo);
				prepararPacote(&pkt, 1, buffer, strlen(buffer));
				check = sendto(socketfd, &pkt, sizeof(pkt), 0, (struct sockaddr *) &serverAddr, serverLen);
			}
			break;
		default:
			break;
	}
	

	close(socketfd);

	return 0;
}
