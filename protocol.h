#define DATA_SIZE_IN_PACKET 1480
#define WINDOW_SIZE  100
#define TIMEOUT		 1000
#define LOSS_RATE	 0.1
#define CORRUPTED_RATE 0.2

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

short calCheckSum(struct TCP_PACKET_FORMAT p){
	int i;
	short sum = 0;
	sum += p.seqNumber + p.ackNumber + p.ackFlag + p.lastFlag + p.dataLength + p.windowSize;
	for (i = 0;i < DATA_SIZE_IN_PACKET; i++)
		sum += packet.data[i];
	return sum;
}