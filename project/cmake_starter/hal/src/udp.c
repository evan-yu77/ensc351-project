#include "hal/udp.h"
#include "hal/sampler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

void* udp_network_thread(void* arg) {

    (void)arg; // Unused parameter
    
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[256];

    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));

    // Bind the socket to the specified port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(UDP_SERVER_PORT);

    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    while (sharedData.is_running) {
        // Receive data from client
        int n = recvfrom(sockfd, (char *)buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&client_addr, &addr_len);
        if (n < 0) {
            perror("recvfrom failed");
            continue;
        }
        buffer[n] = '\0'; // Null-terminate the received data

        char response[256];
        // Prepare the response with the current sensor data
        snprintf(response, sizeof(response), "Accel X: %d, Accel Y: %d", // Current (mA): %d, RPM: %d", 
                 sharedData.accel_x, sharedData.accel_y); // sharedData.current_ma, sharedData.rpm);
        sendto(sockfd, response, strlen(response), 0, (const struct sockaddr *)&client_addr, addr_len);
    }

    close(sockfd);
    return NULL;
}