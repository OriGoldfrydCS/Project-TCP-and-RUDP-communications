# Reliable User Datagram Protocol (RUDP) and Transmission Control Protocol (TCP) Implementation

This project implements both Reliable User Datagram Protocol (RUDP) and Transmission Control Protocol (TCP). The RUDP implementation uses the **STOP-and-WAIT** protocol to ensure reliable data transfer over an unreliable network, adding reliability features such as packet acknowledgment and retransmission, while maintaining the low overhead of UDP. TCP is implemented using its native reliable, connection-based protocol.

## Theoretical Background

### Transmission Control Protocol (TCP)
TCP is a widely used, connection-oriented protocol designed to provide reliable, ordered, and error-checked delivery of data between applications. TCP includes built-in features such as:
- **Flow Control**: Prevents the sender from overwhelming the receiver by dynamically adjusting the rate of data transmission.
- **Congestion Control**: Manages the transmission rate to avoid congesting the network.
- **Retransmissions**: Automatically resends lost or corrupted packets based on timeout and acknowledgment mechanisms.
- **Error Checking**: Uses checksums to detect errors in transmitted packets.

### Reliable User Datagram Protocol (RUDP)
RUDP is an extension of the User Datagram Protocol (UDP) that introduces mechanisms to provide reliable data transmission over an unreliable network. UDP is a connectionless protocol, meaning it does not guarantee that packets will be delivered, arrive in order, or be error-free. RUDP overcomes these limitations by adding reliability features, similar to TCP, while still maintaining UDP’s lightweight nature. Some of the key features of RUDP include:

- **Packet Acknowledgment (ACK)**: The receiver sends an acknowledgment (ACK) for each successfully received packet. If the sender does not receive an ACK within a specified timeout period, it retransmits the packet.
- **Retransmissions**: RUDP handles packet loss by retransmitting packets if an ACK is not received within the timeout window.
- **Sequence Numbers**: Each packet in RUDP is assigned a sequence number to ensure that packets are processed in the correct order. The receiver uses the sequence number to reorder packets if they arrive out of sequence.
- **Error Checking**: Similar to TCP, RUDP can include checksums to ensure data integrity. If the checksum of a received packet does not match the expected value, the packet is discarded, and the sender is requested to retransmit.
- **Connection Control**: Although RUDP operates over UDP, it can still establish connections using SYN, FIN flags (like TCP) to control the start and end of data transmission.

### STOP-and-WAIT Protocol
STOP-and-WAIT is a simple protocol where the sender transmits one packet at a time and waits for an acknowledgment (ACK) from the receiver before sending the next packet. If the ACK is not received within a specified timeout period, the sender retransmits the packet. This ensures reliable delivery but comes at the cost of throughput, especially in high-latency networks. Because the sender waits for an ACK before sending the next packet, only one packet is "in flight" at any given time, which can lead to inefficiencies on high-latency or high-bandwidth networks.

## Project Structure

- `TCP_Sender.c`: Implements the sender side of TCP using socket programming. It handles the transmission of data using TCP’s built-in mechanisms, including flow control, congestion control, and reliable retransmission. The sender establishes a connection with the receiver before data transmission.
- `TCP_Receiver.c`: Implements the receiver side of TCP. The receiver accepts incoming connections from the TCP sender and receives data reliably, ensuring correct order and handling any packet loss or corruption through TCP's automatic retransmission mechanisms.
- `RUDP_Sender.c`: Implements the sender side of RUDP, using the STOP-and-WAIT protocol. It sends one packet at a time, waits for acknowledgment (ACK), and retransmits the packet if an ACK is not received within a timeout period.
- `RUDP_Receiver.c`: Implements the receiver side of RUDP. It listens for incoming packets, sends back acknowledgments for each received packet, and handles any retransmissions in case of packet loss.
- `RUDP_API.c`: Contains utility functions for managing RUDP communication, such as setting up UDP sockets, managing retransmissions, and handling timeouts.
- `RUDP_API.h`: Header file containing the declarations for the RUDP API.
- `makefile`: A makefile to compile the project files into executable binaries.

## RUDP API

The RUDP API includes the following core functions, implemented in `RUDP_API.c`:

- **Socket Setup**: Initializes and configures UDP sockets for sending and receiving packets.
- **Timeout Management**: Implements timeouts for the STOP-and-WAIT protocol to detect packet loss and trigger retransmissions.
- **Retransmission Logic**: Handles the retransmission of packets when an acknowledgment is not received within the specified timeout.
- **Packet Assembly**: Constructs a packet with the header and the payload (data) to be sent to the receiver.
- **Checksum Calculation**: Calculates a checksum to ensure data integrity, which is included in the packet header.

These API functions allow the sender and receiver to communicate reliably over an unreliable transport layer (UDP), ensuring that data is delivered without corruption or loss.

## RUDP Packet Structure

Each packet in RUDP follows a custom structure carrying the necessary metadata required for the reliable transmission of data using the STOP-and-WAIT protocol, along with error detection.

The header includes the following fields:
- **segmentSize**: Indicates the size of the current data segment being transmitted. This helps the receiver understand how much data is in the packet.
- **segmentNumber**: A unique identifier for each segment (or packet), ensuring that packets can be ordered and reassembled correctly.
- **totalSize**: The total size of the entire data transmission. This can be useful in cases where the receiver needs to know the total size of the incoming file or data stream.
- **checksum**: A checksum value is computed for the entire packet to detect any transmission errors. The receiver will recalculate the checksum and compare it to this value to ensure the integrity of the data.
- **flags**: A set of control flags used to indicate the purpose of the packet, such as SYN, ACK, FIN, DATA, and LAST_PACKET.
  
## Usage

### Running TCP

To initiate the TCP connection, you can use the following commands:

1. Start the TCP receiver: ./TCP_Receiver –p <PORT> –algo <ALGO>
2. Run the TCP sender: ./TCP_Sender –ip <IP> –p <PORT> –algo <ALGO>

In this setup:
Replace <PORT> with the port number you want to use.
Replace <IP> with the receiver’s IP address.
Replace <ALGO> with the algorithm you want to use (depending on your wish).

### Running RUDP

To initiate the RUDP connection, you can use the following commands:

1. Start the RUDP receiver: ./RUDP_Receiver –p <PORT>
2. Run the RUDP sender: ./RUDP_Sender –ip <IP> –p <PORT>

In this setup: 
Replace <PORT> with the port number you want to use.
Replace <IP> with the receiver’s IP address.
