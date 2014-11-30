#define DATA_SIZE_IN_PACKET 1480
#define WINDOW_SIZE  100
#define TIMEOUT		 1000
#define LOSS_RATE	 0.1
#define CORRUPTED_RATE 0.2
#define RECEIVING_WAITING_TIME 100000

struct TCP_PACKET_FORMAT{
    int seqNumber;
    int ackNumber;
    char ackFlag;
    char lastFlag;
    short checksum;
    int dataLength;
    char windowSize;
    char data[DATA_SIZE_IN_PACKET];
};

struct WINDOW_FORMAT
{
	struct TCP_PACKET_FORMAT  packet[WINDOW_SIZE];
	clock_t  timer[WINDOW_SIZE];
};

void SendPacket(int sock, struct TCP_PACKET_FORMAT packet){
    int n;
    n = sendto(sock,&packet,sizeof(packet),0,(struct sockaddr *)&cli_addr,clilen);
    if (n < 0) error("ERROR writing packet to socket"); 
}

short calCheckSum(struct TCP_PACKET_FORMAT p){
	int i;
	short sum = 0;
	sum += p.seqNumber + p.ackNumber + p.ackFlag + p.lastFlag + p.dataLength + p.windowSize;
	for (i = 0;i < DATA_SIZE_IN_PACKET; i++)
		sum += p.data[i];
	return sum;
}

int lossByRate(float rate){
    float r;
    srand(time(NULL));
    r = (float)rand()/(float)RAND_MAX;
    if (r < rate)
        return 1;
    return 0;
}

struct TCP_PACKET_FORMAT create_tcp_packet(int seqNumber, int ackNumber, char ackFlag, char lastFlag, int windowSize, 
                                            char *data, int dataSize) 
{
    struct TCP_PACKET_FORMAT tcp_packet;
    int i = 0;

    tcp_packet.seqNumber  = seqNumber;
    tcp_packet.ackNumber  = ackNumber;
    tcp_packet.ackFlag    = ackFlag;
    tcp_packet.lastFlag   = lastFlag;
    tcp_packet.windowSize = windowSize;
    tcp_packet.dataLength = dataSize;

    printf("sequence number = %d\n", seqNumber);      

    bzero(tcp_packet.data, DATA_SIZE_IN_PACKET);
    for (i = 0; i < dataSize; i++) {
       tcp_packet.data[i] = data[i];
    }

    tcp_packet.checksum  = calCheckSum(tcp_packet);
    return tcp_packet;
}

