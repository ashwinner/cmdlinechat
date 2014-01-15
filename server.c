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
int isDuplicateAlias(char *);

struct clientInfoNode {
	int sockFd;
	char *alias;
	struct clientInfoNode * next;
}*head;

#define BUFFER_SIZE 100
#define ALIAS_SIZE 15
#define BACKLOG 10

int main(int argc, char *argv[]) {
	
	head = NULL;
	
	if(argc != 2) {
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
	
	if(listen(sockFd, BACKLOG) < 0) {
		perror("listen");
		exit(1);
	}

	struct sockaddr_storage cliAddr;
	int clilen = sizeof(cliAddr);
	int cliFd;
	
	while(1) {

		if((cliFd = accept(sockFd, (struct sockaddr *)&cliAddr, &clilen)) < 0) { /*Even though the size of an IPv6 address is bigger than the size of
											   of an IPv4 address, it can be cast to the same struct sockaddr. This 
											   is because we will never use the sockaddr directly. Instead, we will
											   check the ss_family field to determine whether the address belongs 
											   to IPv4 or IPv6 and then cast it to a sockaddr_in or sockaddr_in6 
											   respectively, both of which have a sin*_family as the first field. */ 
											  
			perror("accept");
			exit(1);
		}
	
		printf("Client Connected\n");

		pthread_t clientHandler;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		
		if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)!=0) {  //since this thread doesnt need to join later, we can reduce system 
										      //overhead by creating it in this way
			perror("pthread_attr_setdetachedstate");
			exit(1);
	}		
		
		if(pthread_create(&clientHandler, &attr, handleClient, (void *)cliFd)!=0) {
			perror("pthread_create");
			exit(1);
		}
	}
	
	close(sockFd);
	return 0;
}


void addClient(int fd, char *alias) {

	struct clientInfoNode *node = malloc(sizeof(struct clientInfoNode));
	node->sockFd = fd;
	node->alias=alias;
	node->next=NULL;
	
	if(head == NULL) 
		head = node;
	else {
		struct clientInfoNode *ptr;
		for(ptr=head;ptr->next!=NULL;ptr=ptr->next); //find out the last node and append the new node to the end of the list
		ptr->next=node;
	}			
}


void remClient(int fd) {

	struct clientInfoNode *ptr;

	if(head->sockFd == fd) {				//if the node we are looking for is the head node, reassign head
		ptr = head;
		head = ptr->next;
		free(ptr);
	}
	else {				
		for(ptr=head;ptr->next!=NULL;ptr=ptr->next) { 			//otherwise, loop though the list
			if(ptr->next->sockFd == fd) {				//find the node that is just before the required node
				struct clientInfoNode *del = ptr->next;		
				ptr->next = ptr->next->next;			//reassign its next pointer
				free(del);
				break;						//stop looping as soon as you have deleted the element
										//(if you dont do this, you will get a segfault)
			}
		}
	}
}


void * handleClient(void *fd) {

	int cliFd = (int)fd;
	int recvRes;
	char *buf, *alias;
	buf = malloc(BUFFER_SIZE);
	alias = malloc(ALIAS_SIZE);
	
	getAlias(cliFd, alias);		//get an alias for the client					
	
	addClient(cliFd, alias);	//store client's details in the list
	
	broadcast(alias, "I just connected!\n");		//inform everyone that the new client has connected
		
	while(1) {			//keep listening for stuff that the client has to say
	
		recvRes=0;
		memset(buf, 0, BUFFER_SIZE);
	
		if((recvRes = recv(cliFd, buf, BUFFER_SIZE, 0)) < 0) {		//if there's some error in receiving, remove client and close connection
			perror("recv");
			broadcast(alias, "Client disconnected\n");
			remClient(cliFd);
			close(cliFd);
			pthread_exit(NULL);
		}
		
		
		else if(recvRes == 0) {			//client disconnected. remove client and close connection
			broadcast(alias, "Client disconnected\n");
			remClient(cliFd);
			close(cliFd);
			pthread_exit(NULL);
		}
		
		broadcast(alias, buf);			//otherwise, broadcast the stuff that the client sent to everyone, along with his alias
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
	for(ptr=head;ptr!=NULL;ptr=ptr->next) {					//send out the message to everyone in the list
		if(send(ptr->sockFd, newMsg, strlen(newMsg), 0) < 0) {
			perror("send");
		}
	}
} 

void getAlias(int cliFd, char *alias) {

	char *msg1 = "Please provide an alias : ";
	char *msg2 = "Please enter a valid alias : ";
	char *msg3 = "Sorry! That alias is already taken. Please choose a different one : ";
	
	
	if (send(cliFd, msg1, strlen(msg1), 0) < 0 ) {			//first, request an alias
		perror("send");
		pthread_exit(NULL);
	}
	
	do {								//do until the alias the client enters is valid
		memset(alias, 0, ALIAS_SIZE);
		
		if(recv(cliFd, alias, ALIAS_SIZE, 0) < 0) {
			perror("recv");
			pthread_exit(NULL);
		
		}
		
		alias[strlen(alias)-1]='\0';				//every message is terminated by a new line. replace that new line with \0
	
		if(!strcmp(alias, "")) {				//is the alias the empty string?
			if (send(cliFd, msg2, strlen(msg2), 0) < 0 ) {
				perror("send");
				pthread_exit(NULL);
			}
		}
	
		else if(isDuplicateAlias(alias)) {			//is the alias already taken?
			if (send(cliFd, msg3, strlen(msg3), 0) < 0 ) {
				perror("send");
				pthread_exit(NULL);
			}
		}
		
		else 							//its a valid alias!!
			break;
	} while(1);
	
	return;
	
}

int isDuplicateAlias(char *alias) {

	struct clientInfoNode *ptr;
	
	for(ptr=head;ptr!=NULL;ptr=ptr->next) 
		if(!strcasecmp(alias, ptr->alias))
			return 1;
	
	return 0;
}
