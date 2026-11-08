#include "hal/sampler.h"
#include "hal/spi_lib.h"
#include <pthread.h>
#include <time.h>
#include <unistd.h>


SensorData sharedData = {0, 0, 0, 0, true}; // Initialize sharedData with default values

void* background_sampler(void* arg) {
    struct timespec sleep = {0, 10000000}; // 10 milliseconds

    while(sharedData.is_running) {
        // Simulate reading sensor data
        sharedData.accel_x = rand() % 100; // Random value for accel_x
        sharedData.accel_y = rand() % 100; // Random value for accel_y
        
        /* following code will be uncommented when sensors implemented*/
        // sharedData.current_ma = rand() % 500; // Random value for current in milliamps
        // sharedData.rpm = rand() % 3000; // Random value for rpm

        nanosleep(&sleep, NULL); // Sleep for 10 milliseconds
    }

    return NULL;
}