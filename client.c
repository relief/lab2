
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
//char fileName[256];
char *fileName;

struct sockaddr_in serv_addr; 
struct hostent *server; //contains tons of information, including the server's IP address
socklen_t servlen;

int main(int argc, char *argv[])
{
    struct TCP_PACKET_FORMAT tcp_packet;
    struct TCP_PACKET_FORMAT ack_packet;

    int sockfd; //Socket descriptor
    int portno, n;
    char reply[256];
    struct timeb start, end;
    int diff;

    if (argc < 4) {
       fprintf(stderr,"usage %s hostname port filename\n", argv[0]);
       exit(0);
    }
    srand(time(NULL));    
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); //create a new socket
    if (sockfd < 0) 
        error("ERROR opening socket");
    
    server = gethostbyname(argv[1]); //takes a string like "www.yahoo.com", and returns a struct hostent which contains information, as IP address, address type, the length of the addresses...
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    fileName = argv[3];
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; //initialize server's address
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    
    //printf("Please enter the fileName: ");
    //bzero(fileName,256);
    //fgets(fileName,255,stdin);
    fileName[strlen(fileName)] = '\0';
    servlen = sizeof(serv_addr);     

    //n = send(sockfd,buffer,strlen(buffer),0); //send to the socket

    float lossRate, corruptionRate;
    printf("Set the loss rate: ");
    bzero(buffer,256);
    fgets(buffer, 255, stdin);
    sscanf(buffer, "%f", &lossRate);
    if (lossRate < 0 || lossRate > 1) {
        printf("The loss rate must be between 0 and 1\n");
        return 1;
    }

    printf("Set the corruption rate: ");
    bzero(buffer,256);
    fgets(buffer, 255, stdin);
    sscanf(buffer, "%f", &corruptionRate);
    if (corruptionRate < 0 || corruptionRate > 1) {
        printf("The corruption rate must be between 0 and 1\n");
        return 1;
    }

    n = sendto(sockfd,fileName,strlen(fileName),0,(struct sockaddr *)&serv_addr,servlen); //write to the socket
    if (n < 0) 
         error("ERROR writing to socket");
    //bzero(buffer,256);    
    n = recvfrom(sockfd,reply,255,0,(struct sockaddr *)&serv_addr,&servlen);
    if (reply[0] == 'Y')
    {
        ftime(&start);
        dostuff(sockfd,lossRate,corruptionRate);
        outputTimestamp();
        printf("The file was received successfully!\n");
        ftime(&end);
        diff = (int) (1000.0 * (end.time - start.time) + (end.millitm - start.millitm));
        printf("Time elapsed: %u seconds and %u milliseconds\n", diff / 1000, diff % 1000);
    }else
    {
        outputTimestamp();
        printf("The file doesn't exist!\n");
    }
    close(sockfd); //close socket    
    return 0;
}

void send_packet(int sockfd, struct TCP_PACKET_FORMAT packet) {
    int n;
    n = sendto(sockfd,&packet,sizeof(packet),0,(struct sockaddr *)&serv_addr,servlen); //write to the socket
    if (n < 0) error("ERROR writing to socket");
}

int shift_window(struct WINDOW_FORMAT *window, int lastFlag, int *leftMostSeqNum, int *rightMostACKIndex, FILE **fp) {
    // If the smallest window seqNumber is received, shift the window forward by as many received slots as possible
    int firstWaitingWin = 0;
    int i;
    while (window->packet[firstWaitingWin].seqNumber < 0 && firstWaitingWin < WINDOW_SIZE)
        firstWaitingWin += 1;
    if (firstWaitingWin > 0) {
        for (i = 0; i < firstWaitingWin; i++)
        {
            *leftMostSeqNum += DATA_SIZE_IN_PACKET;
            // Append packet about to be shifted out of the window into file; MUST BE IN ORDER
            fwrite(window->packet[i].data, 1, window->packet[i].dataLength, *fp);
            // end after we write the last packet into file
            if (window->packet[i].lastFlag == 1) {
                return 1;
            }
        }

        for (i = 0; i < WINDOW_SIZE; i++)
        {
            if (i + firstWaitingWin <= *rightMostACKIndex)
                window->packet[i] = window->packet[i+firstWaitingWin];
            else
                if (i <= *rightMostACKIndex)
                    window->packet[i].seqNumber = 0;
                else
                    break;
        }
        *rightMostACKIndex -= firstWaitingWin;
    }
    return 0;
}
void disconnect(int sockfd, FILE *fp, int seqNum)
{
    struct TCP_PACKET_FORMAT tcp_packet,ack_packet;
    struct timeval tv;
    tv.tv_usec = RECEIVING_WAITING_TIME * 100;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        perror("Error");
    } 
    ack_packet = create_tcp_packet(0, seqNum, 0, 1, 0, NULL, 0);
    send_packet(sockfd, ack_packet);
    while (recvfrom(sockfd,&tcp_packet,sizeof(tcp_packet),0,(struct sockaddr *)&serv_addr,&servlen) > 0){
        send_packet(sockfd, ack_packet);
    }    
}
void dostuff(int sockfd, float lossRate, float corruptionRate) {
    FILE *fp; // file requested

    int n, i, x;
    struct WINDOW_FORMAT window;
    struct TCP_PACKET_FORMAT tcp_packet, ack_packet;
    int seqNumber, lastFlag, ackNumber, ackFlag, index, leftMostSeqNum,rightMostACKIndex;
    short bytes_read, windowSize;
    char filePath[256];

    seqNumber = 0;
    lastFlag  = 0;
    ackNumber = 0;
    ackFlag   = 0;
    windowSize = WINDOW_SIZE;
    bytes_read = 0;
    leftMostSeqNum = 0;
    rightMostACKIndex = -1;

    // Initialize the client window with blank packets
    for (x = 0; x < windowSize; x++) {
        window.packet[x] = create_tcp_packet(seqNumber, ackNumber, ackFlag, lastFlag, windowSize, NULL, bytes_read);
    }

    // Receive packets from the server
    sprintf(filePath, "rcvd_%s",fileName);
    fp = fopen(filePath, "w");
    while (1)
    {
        n = recvfrom(sockfd,&tcp_packet,sizeof(tcp_packet),0,(struct sockaddr *)&serv_addr,&servlen); //read from the socket
        if (n < 0) {
            error("ERROR reading from socket");
            break;
        }

        // Simulate packet loss by not sending an ACK
        if (isLostCorrupted(lossRate)) {
            outputTimestamp();
            printf("Client: DATA packet %d is lost!\n", tcp_packet.seqNumber);
            continue;
        }

        // Simulate packet corruption by not sending an ACK
        if (isLostCorrupted(corruptionRate)) {
            if (calcCheckSum(tcp_packet) != tcp_packet.checksum) {
                outputTimestamp();
                printf("Client: DATA packet %d is corrupted!\n", tcp_packet.seqNumber);
                continue;
            }
        }      
        outputTimestamp();
        printf("Client: received DATA packet with sequence number %d\n", tcp_packet.seqNumber);
        index = (tcp_packet.seqNumber - leftMostSeqNum) / DATA_SIZE_IN_PACKET ;
        if (index >= WINDOW_SIZE)
            continue;
        if (index >= 0){
            //printf("Received packet's window index = %d\n", index);
            //printf("leftMostSeqNum of window = %d\n", leftMostSeqNum);
            window.packet[index] = tcp_packet;
            window.packet[index].seqNumber = -1;   
            if (index > rightMostACKIndex)
                rightMostACKIndex = index;
            ackNumber = tcp_packet.seqNumber;
        }else{
            ackNumber = leftMostSeqNum;
        }

        // Construct an ACK packet
        ackFlag = 1;
        
        lastFlag = 0;
        windowSize = tcp_packet.rwnd;
        
        // Send the ACK to the server
        ack_packet = create_tcp_packet(tcp_packet.seqNumber, ackNumber, ackFlag, lastFlag, windowSize, NULL, 0);
        send_packet(sockfd, ack_packet);
        outputTimestamp();
        printf("Client: sent ACK %d to server\n", ack_packet.ackNumber);
        
        if (shift_window(&window, lastFlag, &leftMostSeqNum, &rightMostACKIndex, &fp)) {
            // After shifting out the last data packet, we're done
            disconnect(sockfd, fp,leftMostSeqNum - DATA_SIZE_IN_PACKET);
            return;
        }
    }
}
