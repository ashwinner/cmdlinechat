#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>
#include<pthread.h>

void * receiveThread(void *);
void * sendThread(void *);

int main(int argc, char *argv[]) {
	
	if(argc != 2 || atoi(argv[1])==0) {
		fprintf(stderr, "format : %s <port no>\n", argv[0]) ;
		exit(1);
	}
	
	
	struct addrinfo hint, *res;
	memset(&hint, 0, sizeof(hint));
	hint.ai_family=AF_UNSPEC;
	hint.ai_socktype=SOCK_STREAM;

	int getAddrStatus = 0;
	if((getAddrStatus = getaddrinfo("localhost", argv[1], &hint, &res))!=0) {
		fprintf(stderr, "Error getting address info : %s", gai_strerror(getAddrStatus));
		exit(1);
	}
	
	int sockFd;

	if((sockFd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
		perror("socket");
		exit(1);
	}

	if(bind(sockFd, res->ai_addr, res->ai_addrlen) < 0) {
		perror("bind");
		exit(1);
	}
	
	if(listen(sockFd, 5) < 0) {
		perror("listen");
		exit(1);
	}

	struct sockaddr_storage cliAddr;
	int clilen = sizeof(cliAddr);
	int cliFd;
	
	while(1) {

		if((cliFd = accept(sockFd, (struct sockaddr *)&cliAddr, &clilen)) < 0) {
			perror("accept");
			exit(1);
		}
	
		printf("A client has connected!! Enjoy chatting!\n");

		pthread_t sender, receiver;
		
		if(pthread_create(&sender, NULL, sendThread, (void *)cliFd)!=0) {
			perror("pthread_create");
			exit(1);
		}
		
		if(pthread_create(&receiver, NULL, receiveThread, (void *)cliFd)!=0) {
			perror("pthread_create");
			exit(1);
		}

		pthread_join(receiver, NULL);
		pthread_cancel(sender);
		printf("Client Disconnected\n");
		close(cliFd);
	}
	
	close(sockFd);
	return 0;
}

void * sendThread(void *fd) {

	int cliFd = (int)fd;
	int sendRes;
	char sendBuf[50];
	while(1) {
	
		sendRes = 0;
		memset(sendBuf, 0, 50);
	
		fgets(sendBuf, 49, stdin);
	
		if((sendRes = send(cliFd, sendBuf, 50, 0)) < 0) {
			perror("recv");
			pthread_exit(NULL);
		}
	}
}
	

void * receiveThread(void * fd) {

	int cliFd = (int) fd;
	int recvRes;
	char recvBuf[50];
	
	while(1) {
	
		recvRes=0;
		memset(recvBuf, 0, 50);
	
		if((recvRes = recv(cliFd, recvBuf, 50, 0)) < 0) {
				perror("recv");
				pthread_exit(NULL);
		}
		
		else if(recvRes == 0) {
				pthread_exit(NULL);
		}
		
		printf("Client : %s", recvBuf);
	}
}
			 
