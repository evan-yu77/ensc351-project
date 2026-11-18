#include "hal/sampler.h"
#include "hal/spi_lib.h"
#include "hal/motor.h"
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>


static bool fault_printed = false;
SensorData sharedData = {
    .accel_x = 0,
    .accel_y = 0,
    .current_ma = 0.0f,
    .rpm = 0,
    .motor_speed = 0,
    .is_running = true,
    .status = STATUS_OK,
    .fault_active = false,
    .fault_detection_enabled = true
}; // Initialize sharedData with default values

void sampler_clear_fault(void) {
    sharedData.status = STATUS_OK;
    sharedData.fault_active = false;
    fault_printed = false;
    printf("Fault cleared. System is ready.\n");
}

// print the fault diagnostics 

static void print_fault_diagnostics(SystemStatus fault) {
    printf("SYSTEM HALTED: FAULT DETECTED\n");

    switch (fault) {
        case FAULT_CURRENT_OVERFLOW:
            printf("CODE 2: Overcurrent spike (exceeds %.0f mA)\n", THRESHOLD_OVERCURRENT_MA);
            printf("Check for any exposed wires shorting at A01/A02.\n");
            printf("Inspect the motor driver for overheating.\n");
            printf("Verify INA219 I2C calibration math.\n");
            break;
        case FAULT_STALL:
            printf("CODE 1: Motor stalled (high Current, low RPM)\n");
            printf("Check for physical jams on the motor shaft.\n");
            printf("Verify the encoder disk hasn't slipped.\n");
            printf("PWM duty cycle may be too low for the current load.\n");
            break;

        case FAULT_VIBRATION:
            printf("Code 3: SEVERE VIBRATION / IMBALANCE\n");
            printf("Check if motor mount screws have vibrated loose.\n");
            printf("Verify the rotor is centered on the shaft.\n");
            printf("Ensure the accelerometer is secured.\n");
            break;

        default:
            printf("Unknown fault code: %d\n", fault);
            break;
    }
}

static void check_for_faults(void) {
    // skip checking if detection disabled or we are already halted
    if (!sharedData.fault_detection_enabled || sharedData.fault_active) {
        return;
    }

    if (sharedData.current_ma > THRESHOLD_OVERCURRENT_MA) {
        sharedData.status = FAULT_CURRENT_OVERFLOW;
        sharedData.fault_active = true;
    } else if (sharedData.motor_speed > 0 && sharedData.rpm < THRESHOLD_RPM_MIN && sharedData.current_ma > THRESHOLD_STALL_MA) {
        sharedData.status = FAULT_STALL;
        sharedData.fault_active = true;
    } else if (abs(sharedData.accel_x - ACCEL_RESTING_BASELINE) > THRESHOLD_VIBE_MAX || abs(sharedData.accel_y - ACCEL_RESTING_BASELINE) > THRESHOLD_VIBE_MAX) {
        sharedData.status = FAULT_VIBRATION;
        sharedData.fault_active = true;
    }

    // execute safety shutdown

    if (sharedData.fault_active && !fault_printed) {
        motor_stop();
        sharedData.motor_speed = 0;
        print_fault_diagnostics(sharedData.status);
        fault_printed = true;
    }

    // check if system enabled or not 

    
}
void* background_sampler(void* arg) {

    (void)arg; // Unused parameter
    struct timespec sleep = {0, 10000000}; // 10 milliseconds

    while(sharedData.is_running) {

        // Simulate reading sensor data
        sharedData.accel_x = read_adc(0); 
        sharedData.accel_y = read_adc(1); 

        /* following code will be uncommented when sensors implemented*/
        sharedData.current_ma = read_adc(2); 
        // sharedData.rpm = read_adc(3); 
        
        motor_set_dir(true); // Set motor direction to forward
        motor_set_speed(sharedData.motor_speed); // Set motor speed based on sharedData
       
        nanosleep(&sleep, NULL); // Sleep for 10 milliseconds

        check_for_faults(); // fault detection system is active

        usleep(10000); // Sleep for 10 milliseconds
    }

    return NULL;
}

void sampler_enable_fault_detection(bool enabled) {
    sharedData.fault_detection_enabled = enabled;
    if (!enabled) {
        sampler_clear_fault();
    }

    printf("FAULT DETECTION %s.\n", enabled ? "ENABLED" : "DISABLED");
}