#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netdb.h>
#include<pthread.h>

void* receive(void *fd);

int main(int argc, char *argv[]) {

	if(argc != 3) {
	
	fprintf(stderr, "Format: %s <host> <port no>\n", argv[0]);
	exit(1);
	}

	struct addrinfo hint, *res;
	memset(&hint, 0, sizeof(hint));
	hint.ai_family=AF_UNSPEC;
	hint.ai_socktype=SOCK_STREAM;
	
	int getAddrinfoStatus = 0;

	if((getAddrinfoStatus = getaddrinfo(argv[1], argv[2], &hint, &res)) != 0) {

		fprintf(stderr, "Error getting address : %s", gai_strerror(getAddrinfoStatus));
		exit(1);

	}
	
	int sockFd = 0;
	
	if((sockFd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
	
		perror("socket");
		exit(1);
	}

	if(connect(sockFd, res->ai_addr, res->ai_addrlen) < 0 )	{
		
		perror("connect");
		exit(1);
	}

	printf("Connected to server!! Start chatting!\n");
	
	int sendBytes;
	char sendBuf[50];
	pthread_t receiver;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	
	if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)!=0) {
		perror("pthread_attr_setdetachedstate");
		exit(1);
	}
	
	
	if(pthread_create(&receiver, &attr, receive, (void *) sockFd )!=0) {
		perror("pthread_create");
		exit(1);
	}
	
	while(1) {
	
		memset(sendBuf, 0, 50);
		
		sendBytes=0;
	
		fgets(sendBuf, 49, stdin);
		
		if((sendBytes = send(sockFd, sendBuf, strlen(sendBuf)+1, 0)) < 0 ) {
			perror("send");
			exit(1);
		}
	}
	
	close(sockFd);
	return 0;
}

void * receive(void *fd) {

	int sockFd = (int)fd;
	
	char recvBuf[50];
	int recvBytes;
	
	while(1) {
	
		memset(recvBuf, 0, 50);
		recvBytes = 0;
		
		if((recvBytes = recv(sockFd, recvBuf, 50, 0)) < 0) {
			perror("recv");
			exit(2);
		}
		else if (recvBytes == 0) {
			fprintf(stderr, "Server Disconnected\n");
			exit(2);
		}
		
		printf("Server : %s", recvBuf);
		
	}
	
	close(sockFd);
	
	return NULL;
}
