#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/time.h>       // For tv struct
#include "RUDP_API.h"



/*Main function for RUDP sender.*/
/*Return 0 if the program successfully runs, and 1 otherwise*/
int main(int argc, char *argv[])
{

    /*----------------------------------------*/
    /*    Validate Command-Line Arguments     */
    /*----------------------------------------*/

    if (argc != 5)
    {
        print_time("Usage: %s -ip IP -p PORT\n", argv[0]);
        return -1;
    }

    const char *receiver_ip = NULL;
    int receiver_port = 0;

    // Parsing command-line arguments
    for (int i = 1; i < argc; i += 2)
    {
        if (strcmp(argv[i], "-ip") == 0)
            receiver_ip = argv[i + 1];
        else if (strcmp(argv[i], "-p") == 0)
            receiver_port = atoi(argv[i + 1]);
    }

    // Validate that both server_ip and server_port have been properly assigned
    if (receiver_ip == NULL || receiver_port <= 0)
    {
        print_time("Usage: %s -ip IP -p PORT\n", argv[0]);
        return -1;
    }
    printf("\n");
    print_time("Arguments for connection were received successfully\n");


    /*----------------------------------------*/
    /*                Main Code               */
    /*----------------------------------------*/

    // Create a RUDP socket (IPv4, datagram-based, default protocol)
    int sock = -1;
    sock = rudp_socket(AF_INET, SOCK_DGRAM, 0);
    print_time("Sender's RUDP socket opened successfully\n");
    print_time("Initiating connecting process to %s on port %d...\n", receiver_ip, receiver_port);

    // Initialize receiver address structure
    struct sockaddr_in receiver;              // A struct to store the receiver's address
    memset(&receiver, 0, sizeof(receiver));   // Initialize receiver address struct to zeros
    receiver.sin_family = AF_INET;            // Set the address family to Internet Protocol version 4 (AF_INET)
    receiver.sin_port = htons(receiver_port); // Convert the port number to bytes

    // Convert receiver's IP address to binary form
    if (inet_pton(AF_INET, receiver_ip, &receiver.sin_addr) <= 0)

    {
        perror("inet_pton(3)");
        close(sock);
        return 1;
    }

    // Perform RUDP handshake to establish connection
    if (rudp_connect(sock, &receiver) < 0)
    {
        fprintf(stderr, "RUDP connection failed.\n");
        close(sock);
        return 1;
    }

    // Initialize data variables
    int totalDataSent = 0;                         // To store the total data received across all runs
    int segmentNumber = 1;                         // Initialize segment number
    int runs = 0;                                  // Counter for the number of sending cycles
    
    int isSender = 1;                       // A flag to mark the Sender (used for sending FIN at the end of the connection)

    // Main loop for sending up to MAX_RUNS of data transmissions to Receiver
    while(runs < MAX_RUNS)
    {
        int totalSegmentsSent = 0;      // Initialize total segments sent counter
        char filename[20];
        sprintf(filename, "Generate_File_%d.txt", runs + 1);        // Create "filename" to be saved

        // Generate random data to create file content
        util_generate_random_data_file(filename, DATA_SIZE);
        FILE *file = fopen(filename, "rb"); // Reopen the file in read mode to send its contents
        if (file == NULL)                   // Check if file opend successfully
        {
            perror("ERROR: Failed to open file");
            return 1;
        }

        // Get file size
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        double fileSizeInMB = fileSize / (1024.0 * 1024.0);     // Convert file size to MB
        rewind(file);

        printf("---------------------- run #%d ----------------------\n", runs + 1);
        print_time("A %.2f MB file -- '%s' -- generated successfully.\n", fileSizeInMB, filename);

        // Variables to keep tracking on the data transmitted
        int left = DATA_SIZE;
        int offset = 0;
        
        // Variables for sending data over RUDP
        char buffer[MAX_SEGMENT_SIZE];
        size_t bytesRead;

        while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) // Read file contents until the end of it
        {
            int segment_data_size = bytesRead;                           
            int packet_size = sizeof(RUDP_Header) + segment_data_size;   // NOTE: Total packet size equals to the RUDP header plus the data segment's size.

            RUDP_Header *packet = malloc(packet_size);
            if (packet == NULL) 
            {
                print_time("ERROR: Failed to allocate packet.\n");
                break;
            }

            packet->segmentSize = htons(segment_data_size);      // Convert the segment data size to bytes 
            packet->totalSize = htonl(DATA_SIZE);                // Convert the total data size to bytes
            packet->segmentNumber = htonl(segmentNumber);        // Convert the segment number to bytes
            packet->flags = (feof(file) ? LAST_PACKET : DATA);   // Set the packet's flags field (by the EOF function)
            packet->checksum = 0;
            packet->checksum = rudp_compute_checksum(packet, segment_data_size); // Set the original checksum
            
            memcpy(((char*)packet) + sizeof(RUDP_Header), buffer, bytesRead); // Ensure correct offset past the header, avoiding struct padding issues
            // memcpy(packet + 1, buffer, bytesRead);               // Copy the data read from the file into the packet

            // Send the packet to the receiver 
            int sendResult = rudp_send(sock, packet, packet_size, &receiver); 
            if (sendResult == -1) 
            {
                print_time("An error occurred while sending data. Exiting...\n");
                fclose(file);
                rudp_close(sock, &receiver, isSender); // Ensure the socket is closed properly
                return 1; // Exit with an error code
            } 
            else if (sendResult == -2) 
            {
                print_time("Failed to receive ACK after maximum attempts. Exiting...\n");
                fclose(file);
                rudp_close(sock, &receiver, isSender); // Ensure the socket is closed properly
                return 1; // Exit with an error code indicating ACK failure
            }
            else
            {
                // Update transmission variables
                left -= bytesRead;
                offset += bytesRead;
                totalDataSent += bytesRead;
                segmentNumber++;
                totalSegmentsSent++;
                free(packet); 
            }
        }

        fclose(file);
        
        runs++;

        print_time("Data transmission for run #%d completed with ACK's.\n", runs);
        print_time("Total segments sent: %d; Total data sent: %d (bytes); Last segment: #%d\n", totalSegmentsSent + 1, offset, segmentNumber);
        
        // An option to the sender to send more data
        char decision;
        print_time("Do you want to send data again? (y/n): ");
        scanf(" %c", &decision);
        if (decision == 'n' || decision == 'N')
        {
            break;
        }

    }
    
    printf("---------------- close connection ------------------\n");

    // Close RUDP connection and exit
    if (rudp_close(sock, &receiver, isSender) != 0)       // Force the Sender to send FIN before closing socket
    {
        print_time("Failed to close the connection properly\n");
    }
    print_time("Connection closed successfully\n");

    return 0;
}
