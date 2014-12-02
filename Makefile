CC=gcc
CFLAGS = -g 
# uncomment this for SunOS
# LIBS = -lsocket -lnsl

all: client server

client: client.o 
	$(CC) -o client client.o $(LIBS)

server: server.o 
	$(CC) -o server server.o $(LIBS)

client.o: client.c protocol.h

server.o: server.c protocol.h

clean:
	rm -f client server client.o server.o 
