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
        int motorSpeed = 0;
        
        if (sscanf(buffer, "speed %d", &motorSpeed) == 1) {
            if (sharedData.fault_active) {
                snprintf(response, sizeof(response), "ERROR: Active fault (Code %d). Send 'reset' first.\n", sharedData.status);
            } else if (motorSpeed < 0 || motorSpeed > 100) {
                snprintf(response, sizeof(response), "Invalid motor speed. Must be between 0 and 100.\n");
            } else {
                sharedData.motor_speed = motorSpeed;
                snprintf(response, sizeof(response), "Motor speed set to %d\n", motorSpeed);
            }
        } else if (strncmp(buffer, "reset", 5) == 0) {
            sampler_clear_fault();
            snprintf(response, sizeof(response), "Fault cleared. System ready.\n");
        } else if (strncmp(buffer, "fault on", 8) == 0) {
            sampler_enable_fault_detection(true);
            snprintf(response, sizeof(response), "FAULT DETECTION ENABLED\n");
        } else if (strncmp(buffer, "fault off", 9) == 0) {
            sampler_enable_fault_detection(false);
            snprintf(response, sizeof(response), "FAULT DETECTION DISABLED\n");
        } else {
            snprintf(response, sizeof(response), "Accel X: %d, Accel Y: %d, Current (mA): %.1f, RPM: %d, Speed: %d, Fault: %d\n",
                     sharedData.accel_x, sharedData.accel_y, sharedData.current_ma,
                     sharedData.rpm, sharedData.motor_speed, sharedData.status);
        }

        sendto(sockfd, response, strlen(response), 0, (const struct sockaddr *)&client_addr, addr_len);            

        
    }

    close(sockfd);
    return NULL;
}