#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>         // For variadic functions
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/tcp.h>    // For setting congestion control
#include <time.h>
#include <signal.h>


// Define constants for the Sender
#define DATA_SIZE 2097152      // Default size of the data packet sent by the sender (2MB in bytes)


// Auxiliary function declaration (see full implementation below)
void util_generate_random_data_file(const char* filename, unsigned int size);
void print_time(const char *format, ...);


/*Main function for TCP sender.*/
/*Return 0 if the program successfully runs, and 1 otherwise*/
int main(int argc, char *argv[])
{

    /*----------------------------------------*/
    /*    Validate Command-Line Arguments     */
    /*----------------------------------------*/

    if (argc != 7)
    {
        print_time("Usage: %s -ip IP -p PORT -algo ALGO", argv[0]);
        return 1;
    }

    const char *receiver_ip = NULL;
    int receiver_port = 0;
    const char *algo = NULL;

    // Parsing command-line arguments
    for (int i = 1; i < argc; i+=2)
    {
        if (strcmp(argv[i], "-ip") == 0)
            receiver_ip = argv[i + 1];
        else if (strcmp(argv[i], "-p") == 0)
            receiver_port = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-algo") == 0)
            algo = argv[i + 1];
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

    // Create a TCP socket (IPv4, stream-based, default protocol)
    int sock = -1;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket(2)");
    }
    print_time("Sender's TCP socket opened successfully\n");

    // Set congestion control algorithm
    int scc = setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, algo, strlen(algo) + 1);
    if (scc != 0)
    {
        perror("setsockopt(2)");
        close(sock);
        return 1;
    }

    // Set structual data
    struct sockaddr_in receiver;              // A variable that stores the receiver's address.
    memset(&receiver, 0, sizeof(receiver));   // Reset the receiver structure to zeros
    receiver.sin_family = AF_INET;            // Set the receiver's address family to AF_INET (IPv4).
    receiver.sin_port = htons(receiver_port); // Set the receiver's port to the defined port by convertint it to bytes

    // Convert the receiver's address from text to binary form and store it in the receiver structure
    if (inet_pton(AF_INET, receiver_ip, &receiver.sin_addr) <= 0)
    {
        perror("inet_pton(3)");
        close(sock);
        return 1;
    }

    // Connet to the receiver
    print_time("Trying to connect  the server with IP: %s, Port: %d, Algo: %s\n", receiver_ip, receiver_port, algo);
    if (connect(sock, (struct sockaddr *)&receiver, sizeof(receiver)) < 0)
    {
        perror("connect(2)");
        close(sock);
        return 1;
    }
    print_time("TCP 3-way handshake completed!\n");
    print_time("Connection established with %s:%d using %s\n", receiver_ip, receiver_port, algo);

    int file_count = 0;
    long total_bytes_sent;
    char decision = 'y'; 
    int runs = 1;

    while(decision == 'y' || decision == 'Y')
    {
        total_bytes_sent = 0; 
        char filename[256];
        sprintf(filename, "Generate_File_%d.txt", ++file_count);

        // Generate a 2MB file with random data
        util_generate_random_data_file(filename, DATA_SIZE); 

        // Open the file
        FILE* file = fopen(filename, "rb");
        if (!file)
        {
            perror("Failed to open file");
            break;
        }

        // Send the file
        char buffer[4096];
        int bytes_read;

        printf("----------------- run #%d ------------------\n", runs);

        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) 
        {

            // Try to send the data to the receiver
            int bytes_sent = send(sock, buffer, bytes_read, 0);

            if (bytes_sent < 0)
            {
                perror("send(2)");
                close(sock);
                return 1;
            }
            total_bytes_sent += bytes_sent;
        }
        runs++;

        print_time("Data sent successfully. Sent %d bytes to the receiver!\n", total_bytes_sent);

        print_time("Do you want to send another file? (y/n): ");
        scanf(" %c", &decision);
        
    }

    // Send an exit messenge to the receiver
    int exit = send(sock, "EXIT", 4, 0);
    if (exit < 0)
    {
        perror("Failed to send exit messenge");
    }
    else
    {
        printf("--------------------------------------------\n");
        print_time("Sending exit messenge to close the connection...\n");
    }

    close(sock);
    print_time("Connection closed\n");

    return 0;
}

/*----------------------------------------*/
/*          Auxiliary functions           */
/*----------------------------------------*/

/* Utility function to generate a file with random data */
void util_generate_random_data_file(const char* filename, unsigned int size)
{
    FILE* file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file");
        exit(1); // Exit if file cannot be opened
    }

    srand(time(NULL)); // Randomize seed for the random number generator
    for (unsigned int i = 0; i < size; i++) 
    {
        unsigned char random_byte = rand() % 256;
        fwrite(&random_byte, 1, 1, file);
    }

    fclose(file);
}

/*This fucntions helps to show time while printing to terminal*/
void print_time(const char *format, ...)
{
    char formatted_time[9]; // Buffer for HH:MM:SS format
    va_list args;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now); // Take current time

    strftime(formatted_time, sizeof(formatted_time), "%H:%M:%S", tm_info); // Format current time to HH:MM:SS

    va_start(args, format);          // Start processing variable arguments
    printf("[%s] ", formatted_time); // Print the current time prefix
    vprintf(format, args);           // Print the rest of the message with format and args
    va_end(args);                    // Clean up
}
