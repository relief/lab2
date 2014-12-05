/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <fcntl.h>
#include <time.h>
#include "protocol.h"

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
     int sockfd, newsockfd, portno;
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
     
     /****** Kill Zombie Processes ******/
     sa.sa_handler = sigchld_handler; // reap all dead processes
     sigemptyset(&sa.sa_mask);
     sa.sa_flags = SA_RESTART;
     if (sigaction(SIGCHLD, &sa, NULL) == -1) {
         perror("sigaction");
         exit(1);
     }
     /*********************************/

     //while (1) {
         int n = -1;
         clilen = sizeof(cli_addr);         
         bzero(buffer,256);

         n = recvfrom(sockfd,buffer,255,0,(struct sockaddr *)&cli_addr,&clilen);
         dostuff(sockfd); 
     //}

     return 0; /* we never get here */
}

/******** DOSTUFF() *********************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/

void send_packet(int sock, struct TCP_PACKET_FORMAT packet){
    int n;
    n = sendto(sock,&packet,sizeof(packet),0,(struct sockaddr *)&cli_addr,clilen);
    if (n < 0) error("ERROR writing packet to socket"); 
}

/* Shift the window forward as necessary */
int shift_window(struct WINDOW_FORMAT *window, int *packetNum, int lastFlag) {
    // If smallest window seqNumber is acked, shift window forward by as many acked numbers as possible
    int firstWaitingWin = 0;
    int i;
    while (window->packet[firstWaitingWin].seqNumber < 0 && firstWaitingWin < *packetNum) // Take care of buffer overflow
        firstWaitingWin += 1;

    //printf("packetNum = %d, lastFlag = %d\n", packetNum, lastFlag );
    if (firstWaitingWin > 0){
        *packetNum -= firstWaitingWin;
        if (*packetNum == 0 && lastFlag > 0){
            return 1; // error
        }
        for (i = 0; i < *packetNum; i++)
        {
            window->packet[i]   = window->packet[i+firstWaitingWin];
            window->timer[i] = window->timer[i+firstWaitingWin];
        }
    }
    return 0;
}

/* Resend any packets that have timed-out without being ACKed */
void resend_on_timeout(int sock, struct WINDOW_FORMAT *window, int *packetNum) {
    // If timeout, resend
    int i;
    clock_t curTime = clock();
    
    printf("Current time: %lu\n", clock());
    for (i = 0; i < *packetNum; i++)
    {
        printf("Time of %d: %lu\n", i, window->timer[i]);
        if (window->packet[i].seqNumber >= 0 && curTime - window->timer[i] > TIMEOUT)
        {
            printf("Server: resent packet SeqNum %d\n", window->packet[i].seqNumber);
            send_packet(sock,window->packet[i]);
            window->timer[i] = clock();
        }
    }
}

/* Divide file into packets and send them to the receiver */
void send_file_as_packets(int sock, int resource)
{
    char data_to_send[DATA_SIZE_IN_PACKET]; //the packet's data
    int n, i;
    struct WINDOW_FORMAT window;
    struct TCP_PACKET_FORMAT tcp_packet, ack_packet;
    int seqNumber, lastFlag, ackNumber, ackFlag, index, packetNum, firstSeqNum;
    short bytes_read, windowSize;
    struct timeval tv;
    
    tv.tv_usec = RECEIVING_WAITING_TIME;
     if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        perror("Error");
    } 

    packetNum = 0; // the sequence number of the packet, also the packet's index number in the window
    seqNumber = 0;
    lastFlag  = 0;
    ackNumber = 0;
    ackFlag   = 0;
    windowSize= WINDOW_SIZE;

    while (1){

          // Put available packets into window and send them
          while (packetNum < WINDOW_SIZE && lastFlag == 0){
              if ((bytes_read=read(resource, data_to_send, DATA_SIZE_IN_PACKET))>0 ){
                  // Create packet
                  if (bytes_read < DATA_SIZE_IN_PACKET) // if it's the last packet
                      lastFlag = 1;
                  tcp_packet = create_tcp_packet(seqNumber, ackNumber, ackFlag, lastFlag, windowSize, data_to_send, bytes_read);
                  // Save into window
                  window.packet[packetNum] = tcp_packet;
                  window.timer[packetNum] = clock();
                  packetNum += 1;
                  // Send the packet to the client
                  send_packet(sock,tcp_packet);
                  printf("Server: sent packet SeqNum %d\n", seqNumber);
                  seqNumber += DATA_SIZE_IN_PACKET;
              }
          }
          
          firstSeqNum = window.packet[0].seqNumber;
          //Receive ACK
          while(recvfrom(sock,&ack_packet,sizeof(ack_packet),0,(struct sockaddr *)&cli_addr,&clilen) > 0){

              // simulate packet loss by not sending an ACK
              if (isLostCorrupted(LOSS_RATE)) {
                  printf("ACK Packet %d is lost!\n", ack_packet.seqNumber);
                  continue;
              }

              // simulate packet corruption by not sending an ACK
              if (isLostCorrupted(CORRUPTION_RATE)) {
                  printf("ACK Packet %d is corrupted!\n", ack_packet.seqNumber);
                  continue;
              }      

              printf("Server rcvd ackFlag: %d\n",ack_packet.ackFlag);
              printf("Server rcvd ackNumber: %d\n",ack_packet.ackNumber);

              if (ack_packet.ackFlag == 1) {
                  // find the index of the ACK packet in the server's window
                  index = (ack_packet.ackNumber - firstSeqNum) / DATA_SIZE_IN_PACKET ;  // Take care of the change of the first window packet
                  if (index < 0 || index >= WINDOW_SIZE)
                          continue;
                  window.packet[index].seqNumber = -1;  // Marked as ACKed
              }        
          }

          // // If smallest window seqNumber is acked, shift window forward by as many acked numbers as possible
          // firstWaitingWin = 0;
          // while (window.packet[firstWaitingWin].seqNumber < 0 && firstWaitingWin < packetNum) // Take care of buffer overflow
          //     firstWaitingWin += 1;

          // //printf("packetNum = %d, lastFlag = %d\n", packetNum, lastFlag );
          // if (firstWaitingWin > 0){
          //     packetNum -= firstWaitingWin;
          //     if (packetNum == 0 && lastFlag > 0){
          //         break;
          //     }
          //     for (i = 0; i < packetNum; i++)
          //     {
          //         window.packet[i]   = window.packet[i+firstWaitingWin];
          //         window.timer[i] = window.timer[i+firstWaitingWin];
          //     }
          // }

          if (shift_window(&window, &packetNum, lastFlag))
            break;

          resend_on_timeout(sock, &window, &packetNum);
    }
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
   printf("Here is the filePath: %s\n",filePath);
   if ((resource = open(filePath, O_RDONLY)) > 0){
      printf("The file exists. \n");      
      send_file_as_packets(sock, resource);
      printf("The file was sent successfully!\n");
   }
   else{
      printf("The file does not exist! \n");
      output_dne(sock, buffer);
   }
}
