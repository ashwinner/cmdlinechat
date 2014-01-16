#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netdb.h>
#include<pthread.h>

void* receive(void *fd);

#define BUFFER_SIZE 100
#define USERNAME_SIZE 15

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
	
	int sendBytes, recvBytes;
	char buf[BUFFER_SIZE];
		
	char uname[USERNAME_SIZE], password[USERNAME_SIZE];
	
	do {
		
		char *msg1 = "Enter username : ";
		write(1, msg1, strlen(msg1));
		
		fgets(uname, USERNAME_SIZE, stdin);
		
		uname[strlen(uname)-1]='\0';
		
		char c;
		if(sscanf(uname,"user-%*d%c\n", &c)>-1) {
			printf("Invalid username. Username should be of the form \"user-1\" \"user-2\" etc\n");
			continue;
		}
		
		char *msg2 = "Enter password : ";
		write(1, msg2, strlen(msg2));
		
		fgets(password, USERNAME_SIZE, stdin);
	
		password[strlen(password)-1]='\0';
		
		if((sendBytes = send(sockFd, uname, strlen(uname)+1, 0)) < 0 ) {
			perror("send");
			exit(1);
		}
	
		if((sendBytes = send(sockFd, password, strlen(password)+1, 0)) < 0 ) {
			perror("send");
			exit(1);
		}
		
		bzero(buf, BUFFER_SIZE);
	
		if((recvBytes = recv(sockFd, buf, BUFFER_SIZE, 0)) < 0 ) {
			perror("recv");
			exit(1);
		}
		
		else if(recvBytes == 0) {
			fprintf(stderr, "Server Disconnected\n");
			exit(1);
		}
	
		write(1, buf, strlen(buf));
	
	} while(strcasecmp(buf, "login success\n"));
	
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
	
		memset(buf, 0, BUFFER_SIZE);
		
		sendBytes=0;
	
		fgets(buf, BUFFER_SIZE, stdin);
		
		char c;
		sscanf(buf, "%*[Tt]o user-%*d%*c%c", &c);
		if(c!=':') {
			printf("Message should be of the form \"To user-1 :\" \"To user-2 :\" etc\n");
			continue;
		}
		if((sendBytes = send(sockFd, buf, strlen(buf)+1, 0)) < 0 ) {
			perror("send");
			exit(1);
		}
		
	}
	
	close(sockFd);
	return 0;
}

void * receive(void *fd) {

	int sockFd = (int)fd;
	
	char buf[BUFFER_SIZE];
	int recvBytes;
	
	while(1) {
	
		memset(buf, 0, BUFFER_SIZE);
		recvBytes = 0;
		
		if((recvBytes = recv(sockFd, buf, BUFFER_SIZE, 0)) < 0) {
			perror("recv");
			exit(1);
		}
		else if (recvBytes == 0) {
			fprintf(stderr, "Server Disconnected\n");
			exit(1);
		}
		
		write(1, buf, strlen(buf)); //because printf is buffered and wont print until it encounters a new line
		
	}
	
	close(sockFd);
	
	return NULL;
}
