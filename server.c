#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>
#include<pthread.h>
#include<signal.h>

int login(int, char *, char *);
void deactivate(int);
void * handleClient(void *);
void broadcast(char *, char *);
void showActiveUsers(int, char *);


struct clientInfoNode {
	int sockFd;
	char *alias;
	char *password;
	struct clientInfoNode * next;
}*head;

#define BUFFER_SIZE 100
#define ALIAS_SIZE 15
#define BACKLOG 10

int main(int argc, char *argv[]) {
	
	head = NULL;
	signal(SIGPIPE, SIG_IGN);
	
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


int login(int fd, char *alias, char *password) {

	struct clientInfoNode *ptr;
	
	for(ptr=head;ptr!=NULL;ptr=ptr->next) 
		if(!strcmp(alias, ptr->alias))
			if(!strcmp(password, ptr->password)) {
				ptr->sockFd = fd;
				return 1;
			}
			
			else
				return 0;
	
	struct clientInfoNode *node = malloc(sizeof(struct clientInfoNode));
	node->sockFd = fd;
	node->alias=alias;
	node->password=password;
	node->next=NULL;
	
	if(head == NULL) 
		head = node;
	else {
		struct clientInfoNode *ptr;
		for(ptr=head;ptr->next!=NULL;ptr=ptr->next); //find out the last node and append the new node to the end of the list
		ptr->next=node;
	}	
	
	return 1;		
}


void deactivate(int fd) {

	struct clientInfoNode *ptr;

	for(ptr=head;ptr!=NULL;ptr=ptr->next) 
		if(ptr->sockFd == fd) {
			ptr->sockFd=-1;
			return;
		}
}


void * handleClient(void *fd) {

	int cliFd = (int)fd;
	int recvRes, sendRes;
	char *buf, *alias, *password;
	buf = malloc(BUFFER_SIZE);
	alias = malloc(ALIAS_SIZE);
	password = malloc(ALIAS_SIZE);
	
	while(1) {
	
		if((recvRes = recv(cliFd, alias, ALIAS_SIZE, 0)) <= 0 ) {
			perror("recv");
			close(cliFd);
			pthread_exit(NULL);
		}
		
		if((recvRes = recv(cliFd, password, ALIAS_SIZE, 0)) <= 0 ) {
			perror("recv");
			close(cliFd);
			pthread_exit(NULL);
		}
		
		if(!login(cliFd, alias, password)) {
			if((sendRes = send(cliFd, "Login failed\n", 14, 0)) < 0 ) {
				perror("send");
				close(cliFd);
				pthread_exit(NULL);
			}
		}
		
		else {
			if((sendRes = send(cliFd, "Login Success\n", 14, 0)) < 0 ) {
				perror("send");
				close(cliFd);
				pthread_exit(NULL);
			}
			break;
		}
	}
	
	broadcast(alias, "has connected!");		//inform everyone that the new client has connected
	
	showActiveUsers(cliFd, alias);
	
	while(1) {						//keep listening for stuff that the client has to say
	
		recvRes=0;
		bzero(buf, BUFFER_SIZE);
	
		if((recvRes = recv(cliFd, buf, BUFFER_SIZE, 0)) < 0) {		//if there's some error in receiving, remove client and close connection
			perror("recv");
			broadcast(alias, "has disconnected");
			deactivate(cliFd);
			close(cliFd);
			pthread_exit(NULL);
		}
		
		
		else if(recvRes == 0) {			//client disconnected. remove client and close connection
			broadcast(alias, "has disconnected");
			deactivate(cliFd);
			close(cliFd);
			pthread_exit(NULL);
		}
		
		int sendchatRes;
		if((sendchatRes = (sendchat(alias, buf))) < 0) {
		
			if(sendchatRes == -1)
				strcpy(buf, "User is currently not online\n");
			else
				strcpy(buf, "User doesnt exist\n");
				
			send(cliFd, buf, strlen(buf), 0);
		}
	}
}

void broadcast(char *alias, char *msg) {

	char *newMsg;
	newMsg = malloc(strlen(alias) + 1 + strlen(msg) + 2);
	sprintf(newMsg, "%s %s\n", alias, msg);
	newMsg[strlen(newMsg)]='\0';
	struct clientInfoNode *ptr;
	
	for(ptr=head;ptr!=NULL;ptr=ptr->next) {					//send out the message to everyone in the list who is active
		if(ptr->sockFd > 0) 		
			if(send(ptr->sockFd, newMsg, strlen(newMsg), 0) < 0) {
				perror("send");
			}
	}
} 

void showActiveUsers(int cliFd, char *alias) {

	char *msg = "The following users are currently online : \n";
	
	int msglen = strlen(msg);
	
	struct clientInfoNode *ptr;
	for (ptr=head;ptr!=NULL;ptr=ptr->next) {
		if(ptr->sockFd > 0)
			msglen+=strlen(ptr->alias)+1;
	}
	
	char *newMsg = malloc(msglen+1);
	strcpy(newMsg, msg);
	for(ptr=head;ptr!=NULL;ptr=ptr->next) {
		if(ptr->sockFd>0) {
			strcat(newMsg, ptr->alias);
			strcat(newMsg, "\n");
		}
	}
	
	newMsg[strlen(newMsg)]='\0';
	
	if(send(cliFd, newMsg, strlen(newMsg), 0) < 0) {
		perror("send");
		broadcast(alias, "has disconnected\n");
		deactivate(cliFd);
		close(cliFd);
		pthread_exit(NULL);
	}
}

int sendchat (char *senderAlias, char *msg) {

	char *receiverAlias = malloc(ALIAS_SIZE);
	char *actualMsg = malloc(strlen(msg));
	sscanf(msg, "%*s %s : %[^\n]s", receiverAlias, actualMsg);
	actualMsg[strlen(actualMsg)]='\n';
	actualMsg[strlen(actualMsg)+1]='\0';

	struct clientInfoNode *ptr;
	for(ptr=head;ptr!=NULL;ptr=ptr->next) {
		if(!strcmp(receiverAlias, ptr->alias))
			if(ptr->sockFd > 0) {
			
				char *chatMsg = malloc(strlen(senderAlias) + 3 + strlen(actualMsg) + 1);
				sprintf(chatMsg, "%s : %s", senderAlias, actualMsg);
				chatMsg[strlen(chatMsg)]='\0';
				send(ptr->sockFd, chatMsg, strlen(chatMsg), 0);
				return 1;
			}
			else
				return -1;
		}
		
	return -2;
}
