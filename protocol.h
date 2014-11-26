#define DATA_SIZE_IN_PACKET 1480
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