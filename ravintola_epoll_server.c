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
#include <unistd.h>   // close, read
#include <sys/un.h>

#define MAXEVENTS 64
#define SV_SOCK_PATH "mysock"
#define BACKLOG 5
#define BUF_SIZE 256

void error(char *msg)
{
    perror(msg);
    exit(1);
}

void *client_handler(void *arg);
struct epoll_event *ready;  // wait and update???

int main(int argc, char *argv[])
{
    int sfd_un; 
    struct sockaddr_un addr_un; 
    int sfd_in, ne_sfd_in, portno, clilen,epfd;
    int a = 1;
    struct sockaddr_in addr_in, cli_addr;
    struct epoll_event event;    
    pthread_t helper_thread;
    pthread_attr_t thread_attr;

    /* Check arguments */
    if (argc < 2) {
	fprintf(stderr,"ERROR, no port provided\n");
	exit(1);
    }

    /* create UNIX socket locally */
    sfd_un = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd_un == -1)
        perror("Error unix socket");
    /* Construct server socket address, bind socket to it,
       and make this a listening socket */
    if (remove(SV_SOCK_PATH) == -1 && errno != ENOENT)
	perror(SV_SOCK_PATH);
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr_un.sun_family = AF_UNIX;
    strncpy(addr_un.sun_path, SV_SOCK_PATH, sizeof(addr_un.sun_path) - 1);
    if (bind(sfd_un, (struct sockaddr *) &addr_un, sizeof(struct sockaddr_un)) == -1)
        perror("Error unix socket bind");
    if (listen(sfd_un, BACKLOG) == -1)
        perror("Error unix socket listen");

    /* create thread with epoll */
    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr,PTHREAD_CREATE_DETACHED);
    epfd = epoll_create(MAXEVENTS);
    ready = (struct epoll_event*)calloc(MAXEVENTS,sizeof(event));

    /* Create internet socket ??? */
    sfd_in = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd_in < 0)        
	perror("ERROR internet socket create");
    memset((char *) &addr_in ,0,sizeof(addr_in));
    portno = atoi(argv[1]);
    addr_in.sin_family = AF_INET;
    addr_in.sin_addr.s_addr = INADDR_ANY;
    addr_in.sin_port = htons(portno);
    setsockopt(sfd_in, SOL_SOCKET, SO_REUSEADDR, &a, sizeof(int));
    if (bind(sfd_in, (struct sockaddr_in *) &addr_in, sizeof(addr_in)) < 0) 
	perror("ERROR internet socket binding");

    pthread_create(&helper_thread, &thread_attr, client_handler, &epfd);	
    while(1) {
	listen(fd_in,5);
	clilen = sizeof(cli_addr);
	new_sfd_in = accept(fd_in,  
			   (struct sockaddr_in *) &cli_addr, 
			   (socklen_t *)&clilen);
	if (new_sfd_in < 0) { 
	    error("ERROR on accept");
	}	
	event.data.fd = new_sfd_in;
	event.events = EPOLLIN;
	epoll_ctl (epfd, EPOLL_CTL_ADD, new_sfd_in, &event);
    }
    return 0;
}

void * client_handler(void *arg) {
    char buffer[BUF_SIZE];
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
