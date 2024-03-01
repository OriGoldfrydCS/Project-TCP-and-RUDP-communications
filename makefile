# General Macros
CC = gcc
FLAGS = -Wall -g

# Target for compiling all programs
all: TCP RUDP

# Target for TCP program
TCP: TCP_Receiver TCP_Sender

# Target for RUDP program
RUDP: RUDP_Sender RUDP_Receiver

# Targets for dependencies
TCP_Receiver: TCP_Receiver.c
	$(CC) $(FLAGS) TCP_Receiver.c -o TCP_Receiver

TCP_Sender: TCP_Sender.c
	$(CC) $(FLAGS) TCP_Sender.c -o TCP_Sender 

RUDP_Sender: RUDP_Sender.c RUDP_API.c RUDP_API.h 
	$(CC) $(FLAGS) RUDP_Sender.c RUDP_API.c -o RUDP_Sender 

RUDP_Receiver: RUDP_Receiver.c RUDP_API.c RUDP_API.h 
	$(CC) $(FLAGS) RUDP_Receiver.c RUDP_API.c -o RUDP_Receiver

# Clean-up
clean:
	rm -f *.o *.bin *.txt TCP_Receiver TCP_Sender RUDP_Sender RUDP_Receiver