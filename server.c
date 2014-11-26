/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <fcntl.h>
#include "protocol.h"

//struct TCP_PACKET_FORMAT tcp_packet;

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void dostuff(int); /* function prototype */
void error(char *msg)
{
    perror(msg);
    exit(1);
}
char buffer[256];
socklen_t clilen;
struct sockaddr_in cli_addr;

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, pid;
     struct sigaction sa;          // for signal SIGCHLD
     struct sockaddr_in serv_addr;

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_DGRAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
          
     //clilen = sizeof(cli_addr);
     
     /****** Kill Zombie Processes ******/
     sa.sa_handler = sigchld_handler; // reap all dead processes
     sigemptyset(&sa.sa_mask);
     sa.sa_flags = SA_RESTART;
     if (sigaction(SIGCHLD, &sa, NULL) == -1) {
         perror("sigaction");
         exit(1);
     }
     /*********************************/
     
     while (1) {
         int n = -1;
         clilen = sizeof(cli_addr);         
         bzero(buffer,256);
         while (n < 0)
            n = recvfrom(sockfd,buffer,255,0,(struct sockaddr *)&cli_addr,&clilen);
         dostuff(sockfd); 
     }
     return 0; /* we never get here */
}

/******** DOSTUFF() *********************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/

struct TCP_PACKET_FORMAT create_tcp_packet(int seqNumber, int ackNumber, char ackFlag, char lastFlag, int windowSize, 
                                            char *data, int packetSize) 
{
    struct TCP_PACKET_FORMAT tcp_packet;
    int i = 0;

    tcp_packet.seqNumber  = seqNumber;
    tcp_packet.ackNumber  = ackNumber;
    tcp_packet.ackFlag    = ackFlag;
    tcp_packet.lastFlag   = lastFlag;
    tcp_packet.windowSize = windowSize;
    tcp_packet.dataLength = packetSize;

    printf("sequence number = %d\n", seqNumber);      
    //strncpy(tcp_packet.data, data, packetSize);
    //tcp_packet.data[0]   = 'a';
    //tcp_packet.data[1]   = 'o';
    //tcp_packet.data[2]   = '\0';

    bzero(tcp_packet.data, DATA_SIZE_IN_PACKET);
    for (i = 0; i < packetSize; i++) {
      tcp_packet.data[i] = data[i];
    }
    //for (; i < DATA_SIZE_IN_PACKET; i++) {
    //  tcp_packet.data[i] = '/0';
    //}

    return tcp_packet;
}

/* Divide file into packets and send them to the receiver */
void output_header_and_targeted_file_to_sock(int sock, int resource)
{
    int n;
    char data_to_send[1024]; //the packet's data
    int bytes_read;

    struct TCP_PACKET_FORMAT tcp_packet;
    int seqNumber = 1;
    int ackNumber;
    char ackFlag;
    char lastFlag = 0;
    int windowSize;
    
    // Divide target file into 1024-byte packets
    while ((bytes_read=read(resource, data_to_send, DATA_SIZE_IN_PACKET))>0 ){
      if (bytes_read < DATA_SIZE_IN_PACKET) // if it's the last packet
        lastFlag = 1;
      printf("New packet!");
      tcp_packet = create_tcp_packet(seqNumber, ackNumber, ackFlag, lastFlag, windowSize, data_to_send, bytes_read);
      n = sendto(sock,&tcp_packet,sizeof(tcp_packet),0,(struct sockaddr *)&cli_addr,clilen);
      if (n < 0) error("ERROR writing packet to socket");
      seqNumber++;
    }
    printf("bytes_read=%d",bytes_read);
}

/* Output an error when requested file doesn't exist */
void output_dne(int sock, char* fileName)
{
    char str[50];
    int n;
    
    n = sendto(sock,"The file does not exist\n",26,0,(struct sockaddr *)&cli_addr,clilen);
    if (n < 0) error("ERROR writing to socket");
}

void dostuff (int sock)
{
   int n;
   char filePath[256];
   int resource;

   printf("Here is the fileName: %s\n",buffer);
   sprintf(filePath, "resource/%s",buffer);
   printf("Here is the filePath: %s$$$\n",filePath);
   if ((resource = open(filePath, O_RDONLY)) > 0){
      printf("The file exists. \n");      
      output_header_and_targeted_file_to_sock(sock, resource);
   }
   else{
      printf("The file does not exist! \n");
   //   output_dne(sock, fileName);
   }

   n = sendto(sock,"I got your message",18,0,(struct sockaddr *)&cli_addr,clilen);
   if (n < 0) error("ERROR writing to socket");
}
