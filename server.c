#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>
#include<pthread.h>

void addClient(int, char *);
void remClient(int);
void * handleClient(void *);
void broadcast(char *, char *);
void getAlias(int, char *);

struct clientInfoNode {
	int sockFd;
	char *alias;
	struct clientInfoNode * next;
}*head;


int main(int argc, char *argv[]) {
	
	head = NULL;
	
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
	
		printf("Client Connected\n");

		pthread_t clientHandler;
		
		if(pthread_create(&clientHandler, NULL, handleClient, (void *)cliFd)!=0) {
			perror("pthread_create");
			exit(1);
		}
		
	}
	
	close(sockFd);
	return 0;
}


void addClient(int fd, char *alias) {

	struct clientInfoNode *node = malloc(sizeof(struct clientInfoNode));
	memset(node, 0, sizeof(struct clientInfoNode));
	node->sockFd = fd;
	node->alias=alias;
	
	if(head == NULL) 
		head = node;
	else {
		struct clientInfoNode *ptr;
		for(ptr=head;ptr->next!=NULL;ptr=ptr->next); //find out the last element
		ptr->next=node;
	}			
}


void remClient(int fd) {

	struct clientInfoNode *ptr;

	if(head->sockFd == fd) {
		ptr = head;
		head = ptr->next;
		free(ptr);
	}
	else {	
		for(ptr=head;ptr->next!=NULL;ptr=ptr->next) {
			if(ptr->next->sockFd == fd) {
				struct clientInfoNode *del = ptr->next;
				ptr->next = ptr->next->next;
				free(del);
				break;
			}
		}
	}
}


void * handleClient(void *fd) {

	int cliFd = (int)fd;
	int recvRes;
	char *buf, *alias;
	buf = malloc(50);
	alias = malloc(10);
	
	getAlias(cliFd, alias);
	
	addClient(cliFd, alias);
	
	broadcast(alias, "I just connected!\n");
		
	while(1) {
	
		recvRes=0;
		memset(buf, 0, 50);
	
		if((recvRes = recv(cliFd, buf, 50, 0)) < 0) {
			perror("recv");
			remClient(cliFd);
			close(cliFd);
			pthread_exit(NULL);
		}
		
		
		else if(recvRes == 0) {
			remClient(cliFd);
			close(cliFd);
			pthread_exit(NULL);
		}
		
		broadcast(alias, buf);
	}
}

void broadcast(char *alias, char *msg) {

	char *newMsg;
	newMsg = malloc(strlen(alias) + 3 + strlen(msg) + 1);
	memset(newMsg, 0, strlen(alias) + 3 + strlen(msg) + 1);
	
	strcpy(newMsg, alias);
	strcat(newMsg, " : ");
	strcat(newMsg, msg);
	newMsg[strlen(newMsg)]='\0';
	struct clientInfoNode *ptr;
	for(ptr=head;ptr!=NULL;ptr=ptr->next) {
		if(send(ptr->sockFd, newMsg, strlen(newMsg), 0) < 0) {
			perror("send");
		}
	}
} 

void getAlias(int cliFd, char *alias) {

	char *msg = "Please provide an alias : ";
	
	if (send(cliFd, msg, strlen(msg)+1, 0) < 0 ) {
		perror("send");
		pthread_exit(NULL);
	}
	
	if(recv(cliFd, alias, 10, 0) < 0) {
		perror("recv");
		pthread_exit(NULL);
		
	}
	
	alias[strlen(alias)-1]='\0';
	
}

