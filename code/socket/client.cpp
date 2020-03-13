// Client used for testing

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    bool transferComplete = false;

    char buffer[2048576];
    char buf[256];
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
    

    FILE* file;
    file = fopen("./tmp.jpg", "wb");
     
    // Get the message
    printf("Please enter the message (RETR <filename>): ");
    bzero(buffer, sizeof(buffer));
    fgets(buffer, 255, stdin);
      
    // Write it
    n = write(sockfd, buffer, strlen(buffer));
    if(n < 0)
      error("ERROR writing to the socket.");    
    bzero(buffer, sizeof(buffer));

    int prev_write = 0;
    
    // Read the response
    while(1) {
      n = recv(sockfd, buffer, sizeof(buffer), MSG_DONTWAIT);
      if (n > 0) {	  
	fwrite(buffer, 1, n, file);
	printf("n is: %d\n", n);
	//fflush(stdout);
	bzero(buffer, sizeof(buffer));
      }
    }
    
   
    close(sockfd);
    return 0;
}
