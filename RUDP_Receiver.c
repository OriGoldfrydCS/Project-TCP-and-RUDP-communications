#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include "RUDP_API.h"



/*Main function for RUDP receiver.*/
/*Return 0 if the program successfully runs, and 1 otherwise*/
int main(int argc, char *argv[])
{

    /*----------------------------------------*/
    /*    Validate Command-Line Arguments     */
    /*----------------------------------------*/

    if (argc != 3)
    {
        print_time("ERROR! Usage: -p <PORT NUMBER>\n");
        return -1;
    }

    int port = SERVER_PORT;

    // Parsing command-line arguments
    if (strcmp(argv[1], "-p") == 0)
        port = atoi(argv[2]);
    else
    {
        print_time("Error! Usage: -p <PORT NUMBER>\n");
        return -1;
    }

    // Validate that the port number has been properly assigned
    if (port <= 0)
    {
        print_time("ERROR: Invalid port number\n");
        return 1;
    }

    printf("\n");
    print_time("Arguments for connection were received successfully\n");
    // print_time("Port number set to %d\n", port);             // ~~INTERNAL CHECK: Port number validity ~~ //

    /*----------------------------------------*/
    /*                Main Code               */
    /*----------------------------------------*/

    // Create a RUDP socket (IPv4, datagram-based, default protocol)
    int sock = -1;
    sock = rudp_socket(AF_INET, SOCK_DGRAM, 0);
    print_time("Receiver's RUDP socket opened successfully\n");

 
    // Initialize structures for receiver and sender addresses
    struct sockaddr_in receiver;            // A variable that stores the receiver's address information
    struct sockaddr_in sender;              // A variable that stores the sender's address information
    socklen_t sender_len = sizeof(sender);  // A variable that stores the sender's structure length
    memset(&receiver, 0, sizeof(receiver)); // Reset the receiver structures to zeros
    memset(&sender, 0, sender_len);         // Reset the sender structures to zeros

    receiver.sin_family = AF_INET;                // Set the receiver's address family to AF_INET (IPv4)
    receiver.sin_addr.s_addr = htonl(INADDR_ANY); // Set the receiver's address to "0.0.0.0"
    receiver.sin_port = htons(port);              // Set the receiver's port to the specified port

    // Bind the socket to IP address and port
    if (bind(sock, (struct sockaddr *)&receiver, sizeof(receiver)) < 0)
    {
        perror("bind(2)");
        close(sock);
        return 1;
    }
    print_time("RUDP receiver's socket binds successfully\n");
    print_time("Listening to RUDP incoming connections on port %d\n", port);

    // Variables for run statistics
    double *run_times = malloc(sizeof(double));
    double *run_speeds = malloc(sizeof(double));
    if (!run_times || !run_speeds)
    {
        print_time("ERROR: Failed to allocate memory for run statistics!\n");
        free(run_times);
        free(run_speeds);
        close(sock);
        return 1;
    }

    memset(run_times, 0, sizeof(double));
    memset(run_speeds, 0, sizeof(double));

    int runs = 0;                                   // A counter for the number of runs
    long totalDataReceived = 0;                     // A counter for total data received
    int isRunning = 1;                              // A flag for exiting after FIN has sent
    

    // Main loop for run made by the Sender
    while (isRunning && runs < MAX_RUNS)
    {
        RUDP_Header recv_packet;            // Struct for received packet header
        char recv_buffer[DATA_SIZE];        // Buffer for receiving data
        memset(recv_buffer, 0, DATA_SIZE);  // Zero the buffer
        
        // Dynamically allocate memory for the statistic arrays
        double *temp_run_times = realloc(run_times, (runs + 1) * sizeof(double));
        double *temp_run_speeds = realloc(run_speeds, (runs + 1) * sizeof(double));
        if (!temp_run_times || !temp_run_speeds) 
        {
            print_time("ERROR: Failed to resize memory for run statistics!\n");
            free(run_times);
            free(run_speeds);
            close(sock); 
            isRunning = 0;  
            return 1; 
        }
        run_times = temp_run_times;
        run_speeds = temp_run_speeds;

        // File received initializations
        char filename[20];
        snprintf(filename, sizeof(filename), "Received_Run_%d.txt", runs + 1);  // Create "filename" to be saved
        FILE *file = NULL;                                                      // Initiate file pointer
        
        // Variables to monitor the connection
        long runDataReceived = 0;       // A counter for data received in current run
        int lastSegmentReceived = 0;    // A flag for last packet received
        int segmentNumber = 1;          // Segment number within the current run
        int handshakeCompleted = 0;     // A flag to keep track if in connection succeeded
        
        // Structs for storing start and end times
        struct timeval start_time, end_time;
        gettimeofday(&start_time, NULL);        // Record start time         

        if(runs >= 1)                    printf("--------------------------------------------\n");

        // Secondary loop for receiving data (segments) from Sender
        while (!lastSegmentReceived)
        {
            // Prepare a receiving packet
            memset(&recv_packet, 0, sizeof(recv_packet));   // Reset the received packet structures to zeros
            socklen_t sender_len = sizeof(sender);          // Store the size of the sender's address struct
            
            // Try to receive data
            int bytes_received = rudp_recv(sock, recv_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&sender, &sender_len, runs + 1);

            // Check if the sender failed to send data; If so - close both file and socket
            if (bytes_received < 0)     
            {
                perror("recv(2)");
                if (file)      fclose(file);              
                close(sock);
                break;
            }

            RUDP_Header* recv_packet = (RUDP_Header*) recv_buffer;
            
            // Check if ACK received within the 3-way handshake process
            if (recv_packet->flags & ACK && !handshakeCompleted) 
            {
                print_time("*** 3-way handshake completed ***\n");
                print_time("Ready for receiving data...\n");
                printf("--------------------------------------------\n");
                handshakeCompleted = 1; 
                continue; 
            }
           
            // Check if the received bytes are greater than the header -> data to be processed
            if (bytes_received > sizeof(RUDP_Header)) 
            {
                if (!file)                              // Ensure that the file is open for writing
                { 
                    file = fopen(filename, "wb"); 
                    if (!file) 
                    {
                        print_time("ERROR: Failed to open file!");
                        return 1; 
                    }
                }
                fwrite(((char*)recv_packet) + sizeof(RUDP_Header), 1, bytes_received - sizeof(RUDP_Header), file); 
                // fwrite((recv_packet + sizeof(recv_packet)), 1, bytes_received - sizeof(RUDP_Header), file);   // Write the data to the file (only the payload)
                runDataReceived += bytes_received - sizeof(RUDP_Header);                    // Update the run data received counter
                segmentNumber++;                                                            // Increment segment number for the next loop iteration
            }

            // Check if the received packet signals the end of transmission
            if (recv_packet->flags & FIN || lastSegmentReceived || bytes_received == 0 || recv_packet->flags & LAST_PACKET)  // Check if this is the final packet 
            {

                rudp_sendack(sock, &sender, recv_packet->flags, runs + 1);      // Send an acknowledgment for the FIN packet

                if (recv_packet->flags & LAST_PACKET)
                {
                    lastSegmentReceived = 1;                // Mark the last segment to exit the loop
                }
                
                if(recv_packet->flags & FIN)                // Handle the case of closing connection
                {
                    if (file) 
                    {
                        fclose(file);
                        file = NULL;        // Reset file pointer
                    }
                    isRunning = 0;
                    break;
                }
            }
        }

        // Break the loop if FIN sent
        if (!isRunning)
            break;

        // Store run statistics
        gettimeofday(&end_time, NULL);          // Record the end time
        // printf("run #%d\n", runs);           // ~~INTERNAL CHECK: print run number ~~ //       
        
        if (file)                               // Ensure that the file closed
        {
            fclose(file);
            file = NULL;
        }

        // Calculate the time difference between start and end times
        double diff = ((end_time.tv_sec - start_time.tv_sec)*1000) + (((double)(end_time.tv_usec - start_time.tv_usec))/1000);
            
        // Update the run statistics with the calculated time difference and speed
        run_times[runs] = diff;
        run_speeds[runs] = ((double)runDataReceived / 1024 / 1024) / (diff / 1000.0);
        totalDataReceived += runDataReceived;
        
        runs++;

        // ~~INTERNAL CHECK: ensure data gathered correctly ~~ //
        // printf("Up to now:\n");               
        // for (int i = 0; i < runs; ++i) 
        // {
        //     printf("Run #%d: Time: %.3f ms, Speed: %.3f Mbps\n", i + 1, run_times[i], run_speeds[i]);
        // }
    
        print_time("Interim summury (Run %d): %d bytes Sent/%d bytes received by %d segments\n", runs , totalDataReceived / (runs), runDataReceived, segmentNumber);

        // Reset the start and end time structs for the next measurement    
        memset(&start_time, 0, sizeof(start_time));
        memset(&end_time, 0, sizeof(end_time));

        // ~~INTERNAL CHECK: After receiving and saving a run, compare it to the generated file ~~ //
        char generatedFileName[64];
        snprintf(generatedFileName, sizeof(generatedFileName), "Generate_File_%d.txt", runs);
        char receivedFileName[64];
        snprintf(receivedFileName, sizeof(receivedFileName), "Received_Run_%d.txt", runs);
        
        int filesIdentical = compare_files(receivedFileName, generatedFileName);
        if (filesIdentical == 1) 
        {
            print_time("Identity files check (sent vs. received) for Run %d: Files are identical.\n", runs);
        } 
        else if (filesIdentical == 0) 
        {
            print_time("ERROR! Run %d: Files are not identical.\n", runs);
        } 
        else 
        {
            // Error handling if files couldn't be opened
            print_time("ERROR! Run %d: Could not open files for comparison.\n", runs);
        }

        print_time("Waiting to further incoming requests...\n");
    }
    
    // After processing all packets
    if (runs + 1 > 0)   print_statistics(run_times, run_speeds, runs - 1, totalDataReceived); 
    else                print_time("No complete data runs received.\n");
    
    // Clean-up
    free(run_times);
    free(run_speeds);
    print_time("Closing connection and cleaning up...\n");

    int isSender = 0;
    rudp_close(sock, &receiver, isSender);         // Force the Receiver to close sock without sending FIN

    print_time("Receiver end.\n");

    return 0;
}


