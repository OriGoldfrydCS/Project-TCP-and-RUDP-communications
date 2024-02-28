CC = gcc
FLAGS = -Wall -g

all: TCP_Receiver TCP_Sender RUDP_Sender RUDP_Receiver

TCP_Receiver: TCP_Receiver.c
	$(CC) $(FLAGS) TCP_Receiver.c -o TCP_Receiver

TCP_Sender: TCP_Sender.c
	$(CC) $(FLAGS) TCP_Sender.c -o TCP_Sender 

RUDP_Sender: RUDP_Sender.c RUDP_API.c RUDP_API.h 
	$(CC) $(FLAGS) RUDP_Sender.c RUDP_API.c -o RUDP_Sender 

RUDP_Receiver: RUDP_Receiver.c RUDP_API.c RUDP_API.h 
	$(CC) $(FLAGS) RUDP_Receiver.c RUDP_API.c -o RUDP_Receiver

clean:
	rm -f *.o *.bin *.txt TCP_Receiver TCP_Sender RUDP_Sender RUDP_Receiver