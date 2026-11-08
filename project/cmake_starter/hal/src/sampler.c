#include "hal/sampler.h"
#include "hal/spi_lib.h"
#include "hal/motor.h"
#include <pthread.h>
#include <time.h>
#include <unistd.h>


SensorData sharedData = {0, 0, 0, 0, 0, true}; // Initialize sharedData with default values

void* background_sampler(void* arg) {

    (void)arg; // Unused parameter
    struct timespec sleep = {0, 10000000}; // 10 milliseconds

    while(sharedData.is_running) {
        // Simulate reading sensor data
        sharedData.accel_x = read_adc(0); 
        sharedData.accel_y = read_adc(1); 

        /* following code will be uncommented when sensors implemented*/
        sharedData.current_ma = read_adc(2); 
        sharedData.rpm = read_adc(3); 

        motor_set_dir(true); // Set motor direction to forward
        motor_set_speed(sharedData.motor_speed); // Set motor speed based on sharedData

        nanosleep(&sleep, NULL); // Sleep for 10 milliseconds
    }

    return NULL;
}