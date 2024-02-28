#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <stdarg.h>         // For variadic functions
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>    // For TCP specific constants and functions
#include <sys/time.h>       // For getting timestamps
#include <time.h>           // For getting timestamps
#include <stdbool.h>


// Define constants for the Receiver
#define BUFFER_SIZE 2097152 // Default size of the data packet sent by the sender (2MB in bytes)
#define MAX_CLIENTS 1       // Maximum senders to handle parallelly
#define MAX_RUNS 10         // Initial number of processing requests one after the other (we will use dynamic allocation if needed)


// Declaration of auxiliary functions (see full implementation below)
double time_diff(struct timeval x, struct timeval y);
void print_statistics(double *run_times, double *run_speeds, int runs, int totalDataSize, const char *algo);
void save_data_as_txt(const char *data, int size, int run_number);
void print_time(const char *format, ...);
int compare_files(const char *file1, const char *file2);


/*Main function for TCP receiver.*/
/*Return 0 if the program successfully runs, and -1 or 1 otherwise*/
int main(int argc, char *argv[])
{

    /*----------------------------------------*/
    /*    Validate Command-Line Arguments     */
    /*----------------------------------------*/

    if (argc != 5)
    {
        print_time("ERROR! Usage: %s -p PORT -algo ALGO\n", argv[0]);
        return 1;
    }

    const char *algo = NULL;        // To store the congestion control algorithm name
    int port;                       // To store the port number

    // Parsing command-line arguments to get port and algorithm
    for (int i = 1; i < argc; i += 2)
    {
        if (strcmp(argv[i], "-p") == 0)
            port = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-algo") == 0)
            algo = argv[i + 1];
    }

    // Validate that server_port has been properly assigned
    if (port <= 0)
    {
        print_time("ERROR: Invalid port number\n");         
        return 1;
    }
    printf("\n");
    print_time("Arguments for connection were received successfully\n");
    // print_time("Port number set to %d\n", port);                          // ~~INTERNAL CHECK: Port number validity ~~ //

    /*----------------------------------------*/
    /*                Main Code               */
    /*----------------------------------------*/

    // Create a TCP socket (IPv4, stream-based, default protocol)
    int sock = -1;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket(2)");
        return 1;
    }
    print_time("Receiver's TCP socket opened successfully\n");

    // Set congestion control algorithm
    if (algo != NULL)
    {
        if (setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, algo, strlen(algo) + 1) != 0)
        {
            perror("setsockopt: TCP_CONGESTION");
            close(sock);
            return 1;
        }
    }

    // Initialize structures for receiver and sender addresses
    struct sockaddr_in receiver;            // A variable to store the receiver's address
    struct sockaddr_in sender;              // A variable to store the sender's address
    socklen_t sender_len = sizeof(sender);  // A variable that stores the sender's structure length
    memset(&receiver, 0, sizeof(receiver)); // Reset the receiver structures to zeros
    memset(&sender, 0, sender_len);         // Reset the sender structures to zeros

    // Set receiver address to accept any IP and the specified port
    receiver.sin_family = AF_INET;         // Set the receiver's address family to AF_INET (IPv4)
    receiver.sin_addr.s_addr = INADDR_ANY; // Set the receiver's address to "0.0.0.0"
    receiver.sin_port = htons(port);       // Set the receiver's port to the specified port

    // Bind the socket to IP address and port
    if (bind(sock, (struct sockaddr *)&receiver, sizeof(receiver)) < 0)
    {
        perror("bind(2)");
        close(sock);
        return 1;
    }
    print_time("TCP receiver's socket binds successfully\n");

    // Listen for incoming connections
    if (listen(sock, MAX_CLIENTS) < 0)
    {
        perror("listen(2)");
        close(sock);
        return 1;
    }
    print_time("Waiting for incoming TCP connection on port %d...\n", port);

    // Accept a new connetion with a sender
    int sender_sock = accept(sock, (struct sockaddr *)&sender, &sender_len);
    if (sender_sock < 0)
    {
        perror("accept(2)");
        close(sock);
        return 1;
    }
    print_time("Connection established with Sender %s:%d using %s\n", inet_ntoa(sender.sin_addr), ntohs(sender.sin_port), algo);

    // Allocate a buffer for incoming data
    char *buffer = (char *)calloc(BUFFER_SIZE, sizeof(char));
    if (buffer == NULL)
    {
        print_time("Failed to allocate memory for buffer");
        close(sock);
        return 1;
    }
    memset(buffer, 0, BUFFER_SIZE);     // Zero the buffer

    // Initialize variables for receiving data and calculating statistics
    struct timeval start_time, end_time;                // Variables to store start and end times of data reception
    memset(&start_time, 0, sizeof(start_time));         // Zero the structs
    memset(&end_time, 0, sizeof(end_time));
    
    // Statistics variables
    int max_runs = MAX_RUNS;
    double *run_times = (double *)malloc(max_runs * sizeof(double));
    double *run_speeds = (double *)malloc(max_runs * sizeof(double));
    if (!run_times || !run_speeds) 
    {
        print_time("ERROR: Failed to allocate memory for run statistics!\n");
        close(sock);
        free(buffer);
        return 1;
    }
    
    int bytes_received;                                 // The number of bytes received in each recv() cyrcle
    long fileSize = 0;                                  // Tracks the size of the currently processed file 
    long totalDataReceived = 0;                         // Accumulates total data received across all runs
    int runs = 1;                                       // Counter for the number of receive cycles
    bool exitFlag = false;                              // A flag to indicate if EXIT command is received

    // Main loop for receiving up to MAX_RUNS of data transmissions from the Sender
    while (!exitFlag)
    {   
        // Check if reallocation in needed
        if (runs >= max_runs - 1) 
        { 
            max_runs *= 2; 
            run_times = (double *)realloc(run_times, max_runs * sizeof(double));
            run_speeds = (double *)realloc(run_speeds, max_runs * sizeof(double));
            if (!run_times || !run_speeds) 
            {
                print_time("ERROR: Failed to reallocate memory for run statistics!\n");
                exitFlag = true;
                break;
            }
        }

        int left = BUFFER_SIZE;     // Bytes left to receive to complete the current file
        int segmentNumber = 1;      // Segment number within the current run
        bool exitFlag = false;      // A flag to indicate if EXIT command is received
        fileSize = 0;               // Count the file size (ober all iteration)

        printf("--------------------------------------------\n");
        
        gettimeofday(&start_time, NULL);        // Record start time 
        while (left > 0)                        // Iterate until all data for the current file is received or an EXIT command is encountered
        {   
            
            // Receive data
            bytes_received = recv(sender_sock, buffer, BUFFER_SIZE, 0);
            // print_time("Bytes received: %d\n", bytes_received);           // ~~INTERNAL CHECK: bytes received in that iteration ~ //
            
            // Check for disconnect or error
            if (bytes_received <= 0)
            {
                if (bytes_received == 0) print_time("Sender disconnected.\n");
                else perror("recv(2)");
                goto cleanup;
            }

            // Check for Exit command
            if (strncmp(buffer, "EXIT", 4) == 0) 
            {
                print_time("EXIT command received. Exiting...\n");
                exitFlag = true;        // Set flag to break out of loops
                break;
            }            

            // Handle the bytes received
            if (bytes_received > 0)
            {
                // Update counters and totals with the received data
                fileSize += bytes_received;             // Add to current file size
                left -= bytes_received;                 // Decrease remaining data size for current file
                totalDataReceived += bytes_received;    // Add to accumulates total data received across all runs

                // Print details about the received segment
                print_time("Received: Segment #%d\n", segmentNumber);
                print_time("This segement contains %d bytes from the Sender %s:%d\n", bytes_received, inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));
                print_time("The amount of data saved by the Receiver (so far): %d\n", fileSize);
                print_time("Remaining data to send on this run: %d\n", left);

                save_data_as_txt(buffer, bytes_received, runs);     // Save received segment to a file
                printf("----------\n");

                segmentNumber++;        // Increment segment number for the next loop iteration
            }

        } 
        gettimeofday(&end_time, NULL);                       // Record end time 
        
        // Update statistics if data received
        if (fileSize > 0) 
        {
            double dt = ((end_time.tv_sec - start_time.tv_sec)*1000) + (((double)(end_time.tv_usec - start_time.tv_usec))/1000);
            run_times[runs - 1] = dt;
            run_speeds[runs - 1] = ((double)fileSize / 1024 / 1024) / (dt / 1000.0);    
            runs++;     
        }
        
        if (exitFlag)           break; // If EXIT command was received, exit the loop

        print_time("Interim summury (Run #%d): %d bytes Sent/%d bytes received\n", runs - 1, totalDataReceived / (runs - 1), fileSize);
        
        // ~~INTERNAL CHECK: After receiving and saving a run, compare it to the generated file ~~ //
        char generatedFileName[64];
        snprintf(generatedFileName, sizeof(generatedFileName), "Generate_File_%d.txt", runs - 1);
        char receivedFileName[64];
        snprintf(receivedFileName, sizeof(receivedFileName), "Received_Data_Run_%d.txt", runs - 1);
                
        int filesIdentical = compare_files(receivedFileName, generatedFileName);
        if (filesIdentical == 1) 
        {
            print_time("Identity files check (sent vs. received) for Run #%d: Files are identical.\n", runs - 1);
        } 
        else if (filesIdentical == 0) 
        {
            print_time("ERROR! Run #%d: Files are not identical.\n", runs - 1);
        } 
        else 
        {
                // Error handling if files couldn't be opened
                print_time("ERROR! Run #%d: Could not open files for comparison.\n", runs - 1);
        }

        printf("--------------------------------------------\n");
        print_time("Waiting to further incoming connections to come...\n");
        
        memset(buffer, 0, BUFFER_SIZE);     // Clear the buffer for the next run
    }
    
    // Cleanup and print statistics
    cleanup:
    print_statistics(run_times, run_speeds, runs, totalDataReceived, algo);
    print_time("Closing connection and cleaning up...\n");
    print_time("Receiver end.\n");
    close(sock);            // Close Receiver's socket
    close(sender_sock);     // Close Sender's socket

    free(run_times);
    free(run_speeds);
    free(buffer);

    return 0;
}


/*----------------------------------------*/
/*          Auxiliary functions           */
/*----------------------------------------*/

/*Calculates elapsed milliseconds between two time points*/
double time_diff(struct timeval start, struct timeval end)
{
    return (double)(end.tv_sec - start.tv_sec) * 1000.0 + (double)(end.tv_usec - start.tv_usec) / 1000.0;
}

void print_statistics(double *run_times, double *run_speeds, int runs, int totalDataSize, const char *algo)
{
    if (runs <= 0)
    {
        printf("No data to calculate statistics.\n");
        return;
    }

    double total_time_ms = 0;                                   // Total time in milliseconds
    double totalDataSizeMB = totalDataSize / (1024.0 * 1024.0); // Convert bytes to MB
    
    printf("--------------------------------------------\n");
    printf("Detailed Run Statistics:\n");

    // This loop prints individual run statistics
    for (int i = 0; i < runs - 1; ++i)
    {
        
        total_time_ms += run_times[i];
        printf("Run #%d: RTT: %.3lf ms; Speed: %.3lf Mbps\n", i + 1, run_times[i], run_speeds[i]);
    }

    double avg_throughput_MB_s = totalDataSizeMB / (total_time_ms / 1000.0); // Average throughput in Mpss

    printf("--------------------------------------------\n");
    printf("Overall Summary Statistics:\n");
    printf("CC Algorithm: %s\n", algo ? algo : "Default");
    printf("Number of RTT samples: %d\n", runs - 1);
    printf("Overall Data Received: %.3lf MB\n", totalDataSizeMB);
    printf("Average RTT: %.3lf ms\n", total_time_ms / runs);
    printf("Average Throughput: %.3lf Mbps\n", avg_throughput_MB_s);
    printf("Total Time: %.3lf ms\n", total_time_ms);
    printf("---------------------------------------------------------\n");
}

/*This function gets data and saves it to a file (with append mode)*/
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
    print_time("Data from this segment for Run #%d appented and saved to %s\n", run_number, filename);
}

/*This fucntions helps to show time while printing to terminal*/
void print_time(const char *format, ...)
{
    char formatted_time[9]; // Buffer for HH:MM:SS format
    va_list args;
    time_t now = time(NULL);

    struct tm *tm_info = localtime(&now); // Take current time

    strftime(formatted_time, sizeof(formatted_time), "%H:%M:%S", tm_info); // Format current time to HH:MM:SS
    va_start(args, format);                                                // Start processing variable arguments
    printf("[%s] ", formatted_time);                                       // Print the current time prefix
    vprintf(format, args);                                                 // Print the rest of the message with format and args
    va_end(args);                                                          // Clean up
}

/*This functions compares two txt files*/
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