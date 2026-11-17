// Main program to build the application
// Has main(); does initialization and cleanup and perhaps some basic logic.

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "hal/sampler.h"
#include "hal/udp.h"
#include "hal/spi_lib.h"
#include "hal/motor.h"
#include "hal/encoder.h"

int main()
{
    pthread_t sampler_thread, udp_thread;

    printf("Starting vibration diagnostics...\n");

    spi_init(); // Initialize SPI library
    
    encoder_init(522, 525); // Initialize encoder with GPIO pins 23 (channel A, 522) and 24 (channel B, 525)


    motor_init(); // Initialize motor control

    sharedData.is_running = true; // Set the running flag to true

    printf("Listening for UDP commands on port 12345....    \n");

    if (pthread_create(&sampler_thread, NULL, background_sampler, NULL) != 0) {
        printf("Failed to create sampler thread");
        return 1;
    }

    if (pthread_create(&udp_thread, NULL, udp_network_thread, NULL) != 0) {
        printf("Failed to create UDP thread");
        return 1;
    }

    while(sharedData.is_running) {
        sleep(10);
    }


    sharedData.is_running = false;
    pthread_join(sampler_thread, NULL);
    pthread_join(udp_thread, NULL);

    motor_stop();
    encoder_cleanup();
    spi_close(); 
    return 0;
}