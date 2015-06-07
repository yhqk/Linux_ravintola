/* ravintola server 3 
  - Muokkaa ravintola käyttämään säikeitä prosessien tilalta. Rakenne
   pysyy samana.
  - Bonus: Vaihda fifo local socketiin. Tällöin tarvitset printeriin kaksi
    säiettä, joista toinen kuuntelee local socketia kuten internet socketia
    ja lisää connected socketit epoll interest listiin. Toinen säie taas
    kuuntelee epoll_waitilla listaa ja lukee connected socketeista viestit
    ja printtaa ne ulos. Lisäksi client handlerit vaihtavat fifon local
    socketiin ja kirjoittavat siihen. Tarvitset vain yhden local socketin.
*/

#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define MAXEVENTS 64

void error(char *msg)
{
    perror(msg);
    exit(1);
}

void *client_handler(void *arg);
struct epoll_event *ready;

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno, clilen,epfd;
    struct sockaddr_in serv_addr, cli_addr;
    struct epoll_event event;
    
    pthread_t helper_thread;
    pthread_attr_t thread_attr;

    if (argc < 2) {
	fprintf(stderr,"ERROR, no port provided\n");
	exit(1);
    }

    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr,PTHREAD_CREATE_DETACHED);
    epfd = epoll_create(MAXEVENTS);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
	error("ERROR opening socket");
    }

    memset((char *) &serv_addr ,0,sizeof(serv_addr));
    portno = atoi(argv[1]);

    ready = (struct epoll_event*)calloc(MAXEVENTS,sizeof(event));
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    int a = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &a, sizeof(int));
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
	error("ERROR on binding");
    }
    pthread_create(&helper_thread, &thread_attr, client_handler, &epfd);	
    while(1) {
	listen(sockfd,5);
	clilen = sizeof(cli_addr);
	newsockfd = accept(sockfd,  
			   (struct sockaddr *) &cli_addr, 
			   (socklen_t *)&clilen);
	if (newsockfd < 0) { 
	    error("ERROR on accept");
	}	
	event.data.fd = newsockfd;
	event.events = EPOLLIN;
	epoll_ctl (epfd, EPOLL_CTL_ADD, newsockfd, &event);
    }
    return 0;
}

void * client_handler(void *arg) {
    char buffer[256];
    int i;
    int epfd = *((int *)arg);
    ssize_t a; 

    printf("client_handler()\n");
    
    while(1) {
	int n = epoll_wait(epfd,ready,MAXEVENTS,-1); 
	for (i = 0; i < n; i++) {
	    memset(buffer,0,256);
	    a = read(ready[i].data.fd,buffer,256);
	    /* not then removed from ready list*/
	    if (a <= 0) {
		epoll_ctl(epfd, EPOLL_CTL_DEL, ready[i].data.fd, NULL);
		close(ready[i].data.fd);
		printf("Client %d terminated\n",ready[i].data.fd);
	    }	
	    else {
		printf("Client %d says %s\n",ready[i].data.fd,buffer);
	    }
	} 
    }   
}