
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

void error(char *msg)
{
    perror(msg);
    exit(0);
}

void dostuff(int, float, float); /* function prototype */

char buffer[256];
char fileName[256];

struct sockaddr_in serv_addr; 
struct hostent *server; //contains tons of information, including the server's IP address
socklen_t servlen;

int main(int argc, char *argv[])
{
    struct TCP_PACKET_FORMAT tcp_packet;
    struct TCP_PACKET_FORMAT ack_packet;

    int sockfd; //Socket descriptor
    int portno, n;

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
    bzero(fileName,256);
    fgets(fileName,255,stdin);
    fileName[strlen(fileName)-1] = '\0';
    servlen = sizeof(serv_addr);     

    //n = send(sockfd,buffer,strlen(buffer),0); //send to the socket

    float lossRate, corruptionRate;
    printf("Set the loss rate: ");
    bzero(buffer,256);
    fgets(buffer, 255, stdin);
    sscanf(buffer, "%f", &lossRate);

    printf("Set the corruption rate: ");
    bzero(buffer,256);
    fgets(buffer, 255, stdin);
    sscanf(buffer, "%f", &corruptionRate);

    printf("r1=%f,r2 = %f\n", lossRate,corruptionRate );

    n = sendto(sockfd,fileName,strlen(fileName),0,(struct sockaddr *)&serv_addr,servlen); //write to the socket
    if (n < 0) 
         error("ERROR writing to socket");
    //bzero(buffer,256);    
    dostuff(sockfd,lossRate,corruptionRate);
    close(sockfd); //close socket
    
    return 0;
}

void SendPacket(int sockfd, struct TCP_PACKET_FORMAT packet){
    int n;
    n = sendto(sockfd,&packet,sizeof(packet),0,(struct sockaddr *)&serv_addr,servlen); //write to the socket
    if (n < 0) error("ERROR writing to socket");
}

void dostuff(int sockfd, float lossRate, float corruptionRate) {
    FILE *fp; // file requested

    int n, i, x;
    struct WINDOW_FORMAT window;
    struct TCP_PACKET_FORMAT tcp_packet, ack_packet;
    int seqNumber, lastFlag, ackNumber, ackFlag, windowSize, firstWaitingWin, index, bytes_read,leftMostSeqNum;
    //int packetNum = 0;

    seqNumber = 0;
    lastFlag  = 0;
    ackNumber = 0;
    ackFlag   = 0;
    windowSize = WINDOW_SIZE;
    bytes_read = 0;
    firstWaitingWin = 0;
    leftMostSeqNum = 0;

    // Initialize the client window with blank packets
    for (x = 0; x < windowSize; x++) {
        tcp_packet = create_tcp_packet(seqNumber, ackNumber, ackFlag, lastFlag, windowSize, NULL, bytes_read);
        window.packet[x] = tcp_packet;
    }

    // Receive packets from the server
    fp = fopen(fileName, "w");
    while (1)
    {
        n = recvfrom(sockfd,&tcp_packet,sizeof(tcp_packet),0,(struct sockaddr *)&serv_addr,&servlen); //read from the socket
        if (n < 0) {
            error("ERROR reading from socket");
            break;
        }
        // mark window as received

        // simulate packet loss by not sending an ACK
        if (lossCorruptionRate(lossRate)) {
            printf("Packet %d is lost!\n", tcp_packet.seqNumber);
            continue;
        }

        // simulate packet corruption by not sending an ACK
        if (lossCorruptionRate(corruptionRate)) {
            tcp_packet.windowSize -= 10;
            if (calCheckSum(tcp_packet) != tcp_packet.checksum) {
                printf("Packet %d is corrupted!\n", tcp_packet.seqNumber);
                continue;
            }
        }      

        printf("Client: received packet %d with lastFlag = %d\n", tcp_packet.seqNumber, tcp_packet.lastFlag);
        index = (tcp_packet.seqNumber - leftMostSeqNum) / DATA_SIZE_IN_PACKET ;
        if (index < 0 || index >= WINDOW_SIZE){
            continue;
        }
        printf("index = %d\n", index);
        printf("leftMostSeqNum = %d\n", leftMostSeqNum);
        printf("-----------------Old window slot----------------\n");
        printf("window.packet[index].seqNumber = %d\n",window.packet[index].seqNumber);        
        printf("tcp_packet.seqNumber = %d\n",tcp_packet.seqNumber);        
        printf("------------------------------------------------\n");
        window.packet[index] = tcp_packet;
        window.packet[index].seqNumber = -1;
        printf("-----------------New window slot----------------\n");
        printf("window.packet[index].seqNumber = %d\n",window.packet[index].seqNumber);        
        printf("tcp_packet.seqNumber = %d\n",tcp_packet.seqNumber);        
        printf("------------------------------------------------\n");

        //Problem around.... 


        // construct an ACK packet
        ackFlag = 1;
        ackNumber = tcp_packet.seqNumber;
        lastFlag = 1;
        windowSize = tcp_packet.windowSize;
        printf("-----------------------------------------\n");
        printf("ackNumber = %d\n",ackNumber);        
        printf("-----------------------------------------\n");
        // send the ACK
        ack_packet = create_tcp_packet(seqNumber, ackNumber, ackFlag, lastFlag, windowSize, NULL, 0);
        SendPacket(sockfd, ack_packet);
        printf("Client: ACK %d\n", ack_packet.ackNumber);

        // if smallest window seqNumber is received, shift window forward by as many received numbers as possible
        firstWaitingWin = 0;
        while (window.packet[firstWaitingWin].seqNumber < 0 && firstWaitingWin < WINDOW_SIZE)
            firstWaitingWin += 1;
        if (firstWaitingWin > 0) {
            for (i = 0; i < firstWaitingWin; i++)
            {
                leftMostSeqNum += DATA_SIZE_IN_PACKET;
                // Append packet about to be shifted out of the window into file; MUST BE IN ORDER
                fwrite(window.packet[i].data, 1, window.packet[i].dataLength, fp);
                // end after we write the last packet into file
                if (window.packet[i].lastFlag == 1) {
                    close(sockfd);
                    fclose(fp);
                    return;
                }
            }
            for (i = 0; i < WINDOW_SIZE; i++)
            {
                if (i + firstWaitingWin < WINDOW_SIZE)
                    window.packet[i] = window.packet[i+firstWaitingWin];
                else
                    window.packet[i].seqNumber = 0;
            }
        }

    }

}
