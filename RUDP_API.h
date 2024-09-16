#ifndef RUDP_API_H
#define RUDP_API_H

#include <stdint.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1" // Default RUDP's receiver IP address to connect to (overridden by command-line arguments)
#define SERVER_PORT 12345     // Default RUDP's receiver port  to connect to (overridden by command-line arguments)
#define BUFFER_SIZE 2097152   // Default size receiver's buffer (2MB in bytes)  2097152
#define DATA_SIZE 2097152     // Default size of the data packet sent by the sender (2MB in bytes)
#define MAX_SEGMENT_SIZE 1460 // Adjust based on your header size to fit within UDP payload limits
#define MAX_CLIENTS 1         // Maximum senders to handle parallelly by RUDP receiver
#define MAX_ATTEMPTS 1000     // Maximum attempts to send a packet
#define MAX_RUNS 100          // Maximum number of processing requests one after the other
#define TIMEOUT 10            // The maximum wait time for recvfrom(2) call until the sender drops
#define SYN 0x01              // Flag for SYN packets used in handshakes
#define ACK 0x02              // Flag for ACK packets for acknowledgments
#define FIN 0x04              // Flag for FIN packets to close connection
#define DATA 0x08             // flag for data packets
#define LAST_PACKET 0x10      // Flag to indicate the last packet of a run


// RUDP Packet Header struct
typedef struct {
    int segmentSize;                    // Size of the current segment
    int segmentNumber;                  // Segment number for ordering
    unsigned short int totalSize;       // Total data size, if needed
    unsigned short int checksum;        // Checksum for error checking
    char flags;                         // Flags to indicate SYN, ACK, FIN, DATA, and LAST_PACKET
    char __padding[3];
} RUDP_Header;

// Functions for RUDP operations
int rudp_socket(int domain, int type, int protocol);
int rudp_connect(int sock, const struct sockaddr_in *server_addr);
int rudp_send(int socket, RUDP_Header *packet, size_t packet_size, const struct sockaddr_in *dest_addr);
void rudp_send_synack(int socket, const struct sockaddr *src_addr);
void rudp_sendack(int socket, const struct sockaddr_in *addr, int packet_type, int run);
int rudp_recv(int socket, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen, int run);
int rudp_close(int socket, const struct sockaddr_in *server_addr, int isSender);
unsigned short int rudp_compute_checksum(void *data, unsigned int bytes);

// Sender's unique functions declarations
void util_generate_random_data_file(const char* filename, unsigned int size);

// Receiver's unique functions declarations
long time_diff(struct timeval start, struct timeval end);
void save_data_as_txt(const char *data, int size, int run_number);
void print_statistics(double *run_times, double *run_speeds, int runs, int total_data);

// Auxiliary functions declarations
int compare_files(const char *file1, const char *file2);
void print_time(const char *format, ...);

#endif