#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdarg.h>         // For variadic functions
#include "RUDP_API.h"


/********************************************************/
/********************************************************/
/**                                                    **/
/**    Functions used by both Receiver and Sender      **/
/**                                                    **/
/********************************************************/
/********************************************************/

/********************************************************/
/* Create a RUDP socket with the specified domain,      */
/* type and protocol                                    */
/********************************************************/
int rudp_socket(int domain, int type, int protocol)
{
    int sock = socket(domain, type, protocol);
    if (sock < 0)
    {
        perror("socket(2)");
        return -1;
    }
    return sock;
}

/********************************************************/
/* Try to establish a RUDP connection between Receiver  */
/* and Sender with 3-way handshake                      */
/********************************************************/
int rudp_connect(int sock, const struct sockaddr_in *server_addr)
{
    // Initial setup for SYN packet
    RUDP_Header syn_packet;
    memset(&syn_packet, 0, sizeof(syn_packet));         // Zero out the SYN packet struct
    syn_packet.flags = SYN;                             // Set SYN flag for handshake process
    syn_packet.checksum = 0;                            // Initiate checksum to 0
    syn_packet.checksum = rudp_compute_checksum(&syn_packet, sizeof(syn_packet));       // Calculate checksum for the SYN packet

    print_time("Sending SYN packet to Receiver...\n");
    
    // Send SYN to Receiver
    if (sendto(sock, &syn_packet, sizeof(syn_packet), 0, (const struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
    {
        print_time("ERROR: SYN packet send failed!\n");
        return -1;
    }

    // Wait for SYN-ACK response.
    fd_set read_fds;                               // Set of file descriptors to monitor for reading
    struct timeval syn_ack_timeout = {TIMEOUT, 0}; // Timeout for waiting on SYN-ACK
    RUDP_Header syn_ack_packet;                    // Struct for SYN-ACK packet from the Receiver
    struct sockaddr_in syn_ack_from;               // Address of the SYN-ACK sender
    socklen_t from_len = sizeof(syn_ack_from);     // Length of the sender address

    FD_ZERO(&read_fds);      // Initialize the file descriptor set
    FD_SET(sock, &read_fds); // Add the socket to the set

    int select_result = select(sock + 1, &read_fds, NULL, NULL, &syn_ack_timeout);
    if (select_result > 0)
    {
        // SYN-ACK packet received
        if (recvfrom(sock, &syn_ack_packet, sizeof(syn_ack_packet), 0, (struct sockaddr *)&syn_ack_from, &from_len) < 0)
        {
            print_time("ERROR: SYN-ACK packet receive failed!\n");
            return -1;
        }

        // Check if the received packet is SYN-ACK by its flags
        if (syn_ack_packet.flags & ACK)
        {
            print_time("SYN-ACK received from Receiver\n");

            // Send ACK to complete the handshake process
            RUDP_Header ack_response_packet;
            memset(&ack_response_packet, 0, sizeof(ack_response_packet));       // Reset the ACK response packet
            ack_response_packet.flags = ACK;                                    // Set ACK flag
            
            // Checksum check for ACK response packet
            ack_response_packet.checksum = 0;                                  
            ack_response_packet.checksum = rudp_compute_checksum(&ack_response_packet, sizeof(ack_response_packet));  

            if (sendto(sock, &ack_response_packet, sizeof(ack_response_packet), 0, (const struct sockaddr *)&syn_ack_from, from_len) < 0)
            {
                print_time("ERROR: ACK response packet send failed!\n");
                return -1;
            }
            print_time("ACK sent\n");
            print_time("*** 3-way handshake completed ***\n");
        }

        // Handle the situation where no ACK-SYN sent
        else
        {
            print_time("ERROR: Invalid packet received during handshake.\n");
            return -1;
        }
    }

    // If no packet received within the timeout
    else if (select_result == 0)
    {
        print_time("Handshake timeout.\n");
        return -1;
    }

    // Other failure
    else
    {
        print_time("ERROR: select error during handshake\n");
        return -1;
    }

    return 0;
}

/********************************************************/
/* Send a RUDP packet to the specified dest. address    */
/********************************************************/
int rudp_send(int socket, RUDP_Header *packet, size_t packet_size, const struct sockaddr_in *dest_addr)
{
    struct timeval timeout = {TIMEOUT, 0};              // A timeout for waiting on ACK
    fd_set read_fds;                                    // Set of file descriptors
    int attempts = 0;                                   // A counter for the number of transmission attempts
    RUDP_Header ack_packet = {0};                       // A struct to store the received ACK packet
    socklen_t from_len = sizeof(struct sockaddr_in);    // The length of the sender's address structure 
    struct sockaddr_in from_addr;                       // A Struct to store the address of the ACK packet sender

    // Checksum check
    packet->checksum = 0;       
    packet->checksum = rudp_compute_checksum(packet, packet_size);  

    // Set the type of the packet sent
    char packetType[20];
    if (packet->flags & FIN)                strcpy(packetType, "FIN packet");
    else if (packet->flags & DATA)          strcpy(packetType, "Data packet");
    else if (packet->flags & LAST_PACKET)   strcpy(packetType, "Data packet");
    else                                    strcpy(packetType, "Unknown packet type");
    
    // Try to send the packet up to max attempts seted
    while (attempts < MAX_ATTEMPTS)
    {
        // Send the packet to the destination address
        if (sendto(socket, packet, packet_size, 0, (const struct sockaddr *)dest_addr, sizeof(*dest_addr)) < 0)
        {
            print_time("ERROR: Failed to send %s packet!\n", packetType);
            return -1;
        }

        // print_time("%s sent successfully. Waiting for acknowledgment...\n", packetType);   // ~~INTERNAL CHECK: print messege for each segment ~~ //

        FD_ZERO(&read_fds);             // Clear the fd_set before using it again
        FD_SET(socket, &read_fds);      // Add the socket to the fd_set
        
        // Wait for an ACK within the specified timeout window
        int selectResult = select(socket + 1, &read_fds, NULL, NULL, &timeout);

        if (selectResult > 0)
        {
            // Try to receive the ACK packet
            if (recvfrom(socket, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&from_addr, &from_len) >= 0)
            {
                // checksum check
                unsigned short int original_checksum = ack_packet.checksum;                                         // Original checksum
                ack_packet.checksum = 0;                         
                unsigned short int calculated_checksum = rudp_compute_checksum(&ack_packet, sizeof(ack_packet));    // Calculated checksum 

                // Validate the checksum and the ACK flag
                if (calculated_checksum == original_checksum && (ack_packet.flags & ACK))
                {
                    return 0;       // ACK received
                }
            }
        }

        // Handle timeout in case of waiting for ACK
        else if (selectResult == 0)
        {
            print_time("Timeout waiting for ACK for %s, attempt %d/%d\n", packetType, attempts + 1, MAX_ATTEMPTS);
        }
        attempts++;

        if (attempts >= MAX_ATTEMPTS)
        {
            print_time("Maximum retransmission attempts reached for %s packet...\n", packetType);
            return -2;          // Failed to receive ACK
        }
    }

    // Return an error code indicating failure to receive ACK
    print_time("ERROR: Failed to receive ACK after %d attempts!\n", MAX_ATTEMPTS);
    return -2; 
}

/********************************************************/
/* Function to send an ACK packet back to the sender    */
/********************************************************/
void rudp_sendack(int socket, const struct sockaddr_in *addr, int packet_type, int run)
{
    RUDP_Header ack_packet = {0};           // Initiate ACK packet struct
    ack_packet.flags = ACK;                 // Set ACK flag
    
    // Checksum check
    ack_packet.checksum = 0;               
    ack_packet.checksum = rudp_compute_checksum(&ack_packet, sizeof(ack_packet));

    // Try to send the ACK packet
    if (sendto(socket, &ack_packet, sizeof(ack_packet), 0, (const struct sockaddr *)addr, sizeof(*addr)) < 0) 
    {
        print_time("ERROR: Failed to send ACK!\n");
    } 
    
    // Handle each of the scenarios
    else 
    {
        switch (packet_type) 
        {
            case DATA:                                                 // ~~INTERNAL NOTE: Since thousands of segements received 
            //     print_time("ACK for DATA packet sent.\n");          // for each txt file, INTERNAL NOTE: Since thousands of segments received for     
                break;                                                 // each txt file, this case shut down for printing to terminal ~~ // 
            case LAST_PACKET:
                print_time("ACK for all DATA segments in run #%d sent.\n", run);
                break;
            case FIN:
                print_time("ACK for FIN packet sent\n");
                break;
            default:
                print_time("ACK sent for packet type %d.\n", packet_type);
                break;
        }
    }
}

/********************************************************/
/* Function to receive a RUDP packet (SYN, FIN & ACK)   */
/********************************************************/
int rudp_recv(int socket, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen, int run)
{   
    // Attempt to receive a packet
    int recv_bytes = recvfrom(socket, buf, len, flags, src_addr, addrlen);
    if (recv_bytes < 0)
    {
        print_time("ERROR: Receive failed!\n");
        return -1;
    }
    
    // printf("Bytes received: %d \n", recv_bytes);            // ~~INTERNAL CHECK: print the bytes received for each segment ~~ //
    
    RUDP_Header *packet = (RUDP_Header *)buf;        // Set the received buffer as an RUDP packet
    
    // Checksum check
    int original_checksum = packet->checksum; 
    packet->checksum = 0; 
    int calculated_checksum = rudp_compute_checksum(packet, recv_bytes); 

    // Validite checksum received
    if (original_checksum != calculated_checksum)
    {
        print_time("ERROR: Checksum mismatch: original %hu, calculated %hu!\n", original_checksum, calculated_checksum);
        return -1; 
    }
    
    // print_time("Original checksum: %d; Calculated checksum: %d\n", original_checksum, calculated_checksum);     // ~~INTERNAL CHECK: print checksum comparisons ~~ //
    // print_time("Packet flags: %u\n", packet->flags);        // ~~INTERNAL CHECK: print the packet type by its flag(1, 2, 4, etc.) ~~ //

    // Handle the received packet by its flag
    switch(packet->flags)
    {
        case DATA:
            // print_time("Data packet received from sender\n");                                                            // ~~INTERNAL CHECK: print received message for each segment ~~ //
            // print_time("Checksum matches! original: %hu, calculated: %hu\n", original_checksum, calculated_checksum);    // ~~INTERNAL CHECK: print comparison between both checksums for each segment ~~ //
            rudp_sendack(socket, (const struct sockaddr_in *)src_addr, DATA, run); // Send ACK for DATA packet
            break;

        case SYN:
            print_time("SYN packet received, processing...\n");
            rudp_send_synack(socket, src_addr);                              // Send SYN-ACK in response to SYN
            break;

        case FIN:
            print_time("FIN packet received, processing...\n");
            break;

        case ACK:
            print_time("ACK packet received, processing...\n");
            break; 

        case LAST_PACKET:
            print_time("Last data segment within run #%d received, indicating the all the segments received successfully\n", run);
            rudp_sendack(socket, (const struct sockaddr_in *)src_addr, DATA, run); // Treat LAST_PACKET similar to DATA for ACK
            break;

        default:
            // Handle other packet types or invalid packets
            print_time("Received an unknown or invalid packet type!\n");
            return -2;
    }
    return recv_bytes; 
}


/********************************************************/
/* Send a SYN-ACK packet over a socket to a specified   */
/* address. This function used for the 3-way handshake  */
/* in a RUDP protocol to acknowledge a SYN packet and   */
/* indicate readiness for data transmission             */
/********************************************************/
void rudp_send_synack(int sock, const struct sockaddr *src_addr)
{
    RUDP_Header syn_ack_packet = {0};   // Initialize ACK packet structure to zero
    syn_ack_packet.flags = SYN | ACK;   // Set tboth SYN and ACK flags to indicate this is that kind of packet  
    syn_ack_packet.checksum = 0;        // Zero out the checksum for accurate calculation
    syn_ack_packet.checksum = rudp_compute_checksum(&syn_ack_packet, sizeof(syn_ack_packet));            // Compute the packet's checksum for integrity verification
    
    // Send the SYN-ACK packet to the source address
    sendto(sock, &syn_ack_packet, sizeof(syn_ack_packet), 0, src_addr, sizeof(struct sockaddr_in));
    
    print_time("SYN-ACK sent\n");
}

/********************************************************/
/* Initiate closure of an RUDP connection by sending    */
/* a FIN packet to the Receiver and waiting for an ACK  */
/* to ensures the connection is properly closed         */
/********************************************************/
int rudp_close(int sock, const struct sockaddr_in *server_addr, int isSender)
{
    RUDP_Header fin_packet = {0};                       // Initialize FIN packet structures to zero
    struct timeval timeout = {TIMEOUT, 0};              // Set Timeout structure for select

    int socket = sock;
    fd_set read_fds;             // Set of file descriptors for select
    FD_ZERO(&read_fds);          // Clear the file descriptor set
    FD_SET(sock, &read_fds);     // Add the socket file descriptor to the set

    if(isSender == 1)     // Only the Sender sends FIN
    {
        fin_packet.flags = FIN;     // Set FIN flag 
        
        // Checksum check
        fin_packet.checksum = 0;   
        fin_packet.checksum = rudp_compute_checksum(&fin_packet, sizeof(fin_packet));   

        // Try to send the FIN packet and signal connection closure.
        if(sendto(socket, &fin_packet, sizeof(fin_packet), 0, (const struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
        {
            print_time("ERROR: Failed to send FIN packet!\n");
            return -1;
        }
        print_time("FIN sent\n");

        // Wait for an ACK response to the FIN packet within the timeout period.
        if (select(socket + 1, &read_fds, NULL, NULL, &timeout) > 0)
        {
            RUDP_Header ack_packet = {0};   // Initialize a structure to store the received ACK packet
            
            // Check for receiving ACK
            if (recvfrom(socket, &ack_packet, sizeof(ack_packet), 0, NULL, NULL) >= 0) 
            {   
                // Validate the ACK flag
                if (ack_packet.flags & ACK)
                {
                    print_time("ACK for FIN received, closing connection...\n");
                    return close(socket);     
                }
            }
            
            else
            {
                print_time("ERROR: Failed to receive ACK for FIN!\n");
                return -1;    
            }
        }

        // Timeout scenario
        else
        {
            print_time("Timeout waiting for ACK for FIN, closing connection...\n");
            return close(socket); 
        }
    }

    // Close Receiver socket
    if(isSender == 0)       return close(socket);

    return 0;       // successfully closure
}

/********************************************************/
/* Computes the checksum for a given block of data      */
/********************************************************/
unsigned short int rudp_compute_checksum(void *data, unsigned int bytes)
{
    unsigned short int *data_pointer = (unsigned short int *)data;      // Cast data pointer to short int for 16-bit processing
    unsigned int total_sum = 0;                                         // Initialize sum to 0
    
    // Main loop: Sum 16-bit words together, handling overflow by wrapping around
    while (bytes > 1)
    {
        total_sum += *data_pointer++;       // Add 16-bit word to sum and move pointer
        bytes -= 2;                         // Decrement byte count by the size of a short int
    }

    // If there is a left-over byte, add it to the sum
    if (bytes > 0)
        total_sum += *((unsigned char *)data_pointer);

    // Fold 32-bit sum to 16 bits: add carry to the result itself
    while (total_sum >> 16)
        total_sum = (total_sum & 0xFFFF) + (total_sum >> 16);   // Keep folding until sum is reduced to 16 bits

    return (~((unsigned short int)total_sum));
}

/********************************************************/
/* This fucntions shows time while printing to terminal */
/********************************************************/
void print_time(const char *format, ...)
{
    char formatted_time[9];                 // Buffer for HH:MM:SS format
    va_list args;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);   // Measure the current time

    strftime(formatted_time, sizeof(formatted_time), "%H:%M:%S", tm_info);      // Format current time to HH:MM:SS

    va_start(args, format);          // Start processing variable arguments
    printf("[%s] ", formatted_time); // Print the current time prefix
    vprintf(format, args);           // Print the rest of the message with format and args
    va_end(args);                    // Clean up
}

/********************************************************/
/* This fucntions prints statistics after closure       */
/********************************************************/
void print_statistics(double *run_times, double *run_speeds, int runs, int totalDataSize)
{
    if (runs <  0 || totalDataSize <= 0)
    {
        printf("No data to calculate statistics.\n");
        return;
    }
    
    double total_time_ms = 0.0;
    double totalDataSizeMB = totalDataSize / (1024.0 * 1024.0);     // Convert total bytes to MB

    printf("--------------------------------------------\n");
    printf("Detailed Run Statistics:\n");

    for (int i = 0; i < runs + 1; i++)
    {
        total_time_ms += run_times[i];
        printf("Run #%d:\t RTT: %.3f ms; Speed: %.3f Mbps\n", i + 1, run_times[i], run_speeds[i]);
    }

    double avg_throughput_MB_s = totalDataSizeMB / (total_time_ms / 1000.0); // Average throughput in Mpss

    printf("--------------------------------------------\n");
    printf("Overall Summary Statistics:\n");
    printf("Number of RTT samples: %d\n", runs + 1);
    printf("Overall Data Received: %.3f MB\n", totalDataSizeMB);
    printf("Average RTT: %.3f ms\n", total_time_ms / (runs + 1));
    printf("Average Throughput: %.3f Mbps\n", avg_throughput_MB_s);
    printf("Total Time: %.3f ms\n", total_time_ms);
    printf("--------------------------------------------\n");
}

/********************************************************/
/********************************************************/
/**                                                    **/
/**        Functions exclusively used by Sender        **/
/**                                                    **/
/********************************************************/
/********************************************************/

/********************************************************/
/* Utility function to generate random data as txt      */
/********************************************************/
void util_generate_random_data_file(const char* filename, unsigned int size)
{
    FILE* file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file");
        exit(1);        // Exit if file cannot be opened
    }

    srand(time(NULL));  // Randomize seed for the random number generator
    for (unsigned int i = 0; i < size; i++)     // Generate random data
    {
        unsigned char random_byte = rand() % 256;
        fwrite(&random_byte, 1, 1, file);
    }

    fclose(file);
}

/********************************************************/
/********************************************************/
/**                                                    **/
/**       Functions exclusively used by Receiver       **/
/**                                                    **/
/********************************************************/
/********************************************************/

/********************************************************/
/* This function saves the transmitted data to file     */
/********************************************************/
void save_data_as_txt(const char *data, int size, int run_number)
{
    char filename[50];
    sprintf(filename, "Received_Data_Run_%d.txt", run_number);
    FILE *file = fopen(filename, "a");
    if (!file)
    {
        perror("Failed to open file");
        return;
    }
    fwrite(data, 1, size, file);
    fclose(file);
    print_time("Data from Run %d saved to %s\n", run_number, filename);
}

/********************************************************/
/* This function calculates elapsed milliseconds        */
/* between two time points                              */
/********************************************************/
long time_diff(struct timeval start, struct timeval end) 
{
    long sec_diff = end.tv_sec - start.tv_sec;
    long usec_diff = end.tv_usec - start.tv_usec;
    if (usec_diff < 0) 
    {
        sec_diff -= 1;
        usec_diff += 1000000;                   // Adjust microseconds
    }
    return sec_diff * 1000 + usec_diff / 1000;  // Convert to milliseconds
}

/********************************************************/
/* This function compare two files:                     */
/* file1 = Sender's file; file2 = Rececived data        */
/********************************************************/
int compare_files(const char *file1, const char *file2) 
{
    FILE *fp1 = fopen(file1, "rb");
    FILE *fp2 = fopen(file2, "rb");
    if (fp1 == NULL || fp2 == NULL) 
    {
        if (fp1) fclose(fp1);
        if (fp2) fclose(fp2);
        return -1; 
    }

    int isIdentical = 1;                    // A flag to identity
    char ch1, ch2;
    while (!feof(fp1) && !feof(fp2)) 
    {
        fread(&ch1, sizeof(char), 1, fp1);
        fread(&ch2, sizeof(char), 1, fp2);

        if (ch1 != ch2) 
        {
            isIdentical = 0;                // Files are not identical
            break;
        }
    }

    // Check for any remaining bytes in either file
    if (!feof(fp1) || !feof(fp2)) {
        isIdentical = 0;                    // Files are not identical
    }

    fclose(fp1);
    fclose(fp2);

    return isIdentical;
}