#ifndef SAMPLER_H
#define SAMPLER_H

#include <stdbool.h>


// IMPORTANT: ALL OF THESE ARE CURRENTLY JUST PLACEHOLDER VALUES. PHYSICAL TESTING IS REQUIRED TO DETERMINE THE ACTUAL VALUES LATER
#define THRESHOLD_OVERCURRENT_MA 1000.0f // hard current limit for short circuits
#define THRESHOLD_STALL_MA 600.0f // current draw when shaft is jammed
#define THRESHOLD_RPM_MIN 15  // min expected RPM when motor is spinning (speed > 0)
#define THRESHOLD_VIBE_MAX 150 // max allowed deviation from resting accelerometer readings 
#define ACCEL_RESTING_BASELINE 2048 // midpoint for 12-bit ADC (MCP3208)

typedef enum {
    STATUS_OK = 0,
    FAULT_STALL = 1,
    FAULT_CURRENT_OVERFLOW = 2,
    FAULT_VIBRATION = 3
} SystemStatus; 

typedef struct {
    int accel_x;
    int accel_y;
    float current_ma; // current in milliamps
    int rpm;
    int motor_speed; // track motor speed
    bool is_running;

    // fault tracking 
    SystemStatus status;
    bool fault_active;
    bool fault_detection_enabled; // feature for enabling and disabling fault detector
} SensorData;

extern SensorData sharedData;

void* background_sampler(void* arg);
void sampler_init(void);
void sampler_clear_fault(void);
void sampler_enable_fault_detection(bool enabled);

#endif 