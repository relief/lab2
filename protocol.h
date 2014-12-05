#include <stdlib.h>  // rand(), srand()
#include <time.h> 
#include <sys/timeb.h>

#define DATA_SIZE_IN_PACKET 1008
#define WINDOW_SIZE  1000
#define TIMEOUT		 100
#define RECEIVING_WAITING_TIME 100

/* The TCP packet consists of a header and a payload
 * seqNumber: sequence number
 * ackNumber: ACK number
 * ackFlag: 1 if the packet is an ACK packet, 0 for regular packet
 * lastFlag: 1 if the packet is the last packet, 0 for all other packets
 * checksum: used for corruption detection
 * dataLength: length in bytes of the data sent
 * rwnd: the receiving window size of the client or server sending the packet
 * data: the payload
 */
struct TCP_PACKET_FORMAT{
    int seqNumber;
    int ackNumber;
    char ackFlag;
    char lastFlag;
    short checksum;
    short dataLength;
    short rwnd;
    char data[DATA_SIZE_IN_PACKET];
};

/* The window stores each packet and keeps a timer for each of them */
struct WINDOW_FORMAT
{
	struct TCP_PACKET_FORMAT packet[WINDOW_SIZE];
	clock_t timer[WINDOW_SIZE];
};

/* Calculates the checksum of a packet */
short calcCheckSum(struct TCP_PACKET_FORMAT p){
	int i;
	short sum = 0;
	sum += p.seqNumber + p.ackNumber + p.ackFlag + p.lastFlag + p.dataLength + p.rwnd;
	for (i = 0;i < DATA_SIZE_IN_PACKET; i++)
		sum += p.data[i];
	return sum;
}

/* Determines if a loss or corruption occurs according to the input rate */
int isLostCorrupted(float rate){
    float r;
    r = (float)rand()/(float)RAND_MAX;
    if (r < rate)
        return 1;
    return 0;
}

/* Creates a TCP packet */
struct TCP_PACKET_FORMAT create_tcp_packet(int seqNumber, int ackNumber, char ackFlag, char lastFlag, short windowSize, 
                                            char *data, short dataSize) 
{
    struct TCP_PACKET_FORMAT tcp_packet;
    int i = 0;

    tcp_packet.seqNumber  = seqNumber;
    tcp_packet.ackNumber  = ackNumber;
    tcp_packet.ackFlag    = ackFlag;
    tcp_packet.lastFlag   = lastFlag;
    tcp_packet.dataLength = dataSize;
    tcp_packet.rwnd = windowSize;

    bzero(tcp_packet.data, DATA_SIZE_IN_PACKET);
    for (i = 0; i < dataSize; i++) {
       tcp_packet.data[i] = data[i];
    }

    tcp_packet.checksum  = 0; //calcCheckSum(tcp_packet);
    return tcp_packet;
}
void outputTimestamp()
{
  struct timeb tmb;
  struct tm *ptm;
 
  ftime(&tmb);
  ptm = localtime(&tmb.time);
  printf("Current time: %d:%d:%d %d ms\n    ", ptm->tm_hour, ptm->tm_min, ptm->tm_sec, tmb.millitm);
}

