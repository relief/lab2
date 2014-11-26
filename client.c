
/*
 A simple client in the internet domain using TCP
 Usage: ./client hostname port (./client 192.168.0.151 10000)
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      // define structures like hostent
#include <stdlib.h>
#include <string.h>
#include "protocol.h"

struct TCP_PACKET_FORMAT tcp_packet;

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd; //Socket descriptor
    int portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server; //contains tons of information, including the server's IP address
    char buffer[256];
    FILE *fp; // file requested

    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); //create a new socket
    if (sockfd < 0) 
        error("ERROR opening socket");
    
    server = gethostbyname(argv[1]); //takes a string like "www.yahoo.com", and returns a struct hostent which contains information, as IP address, address type, the length of the addresses...
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; //initialize server's address
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    
    printf("Please enter the fileName: ");
    bzero(buffer,256);
    fgets(buffer,255,stdin);
    buffer[strlen(buffer)-1] = '\0';
    socklen_t servlen = sizeof(serv_addr);     

    //n = send(sockfd,buffer,strlen(buffer),0); //send to the socket
    n = sendto(sockfd,buffer,strlen(buffer),0,(struct sockaddr *)&serv_addr,servlen); //write to the socket
    if (n < 0) 
         error("ERROR writing to socket");

    //bzero(buffer,256);

    // Receive packets from the server
    fp = fopen(buffer, "w");
    do
    {
        n = recvfrom(sockfd,&tcp_packet,sizeof(tcp_packet),0,(struct sockaddr *)&serv_addr,&servlen); //read from the socket
        if (n < 0) 
              error("ERROR reading from socket");
        printf("%d\n",tcp_packet.seqNumber);
        printf("%d\n",tcp_packet.ackNumber);
        printf("%d\n",tcp_packet.ackFlag);
        printf("%d\n",tcp_packet.lastFlag);
        printf("%s\n",tcp_packet.data);
        // Append packet into file
        fwrite(tcp_packet.data, 1, tcp_packet.dataLength, fp);        
    } while (tcp_packet.lastFlag == 0); //keep reading files from server until reaches the last packet
    fclose(fp);

    close(sockfd); //close socket
    
    return 0;
}
