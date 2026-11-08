#ifndef SAMPLER_H
#define SAMPLER_H

#include <stdbool.h>

typedef struct {
    int accel_x;
    int accel_y;
    int current_ma; // current in milliamps
    int rpm;
    int motor_speed; // track motor speed
    bool is_running;
} SensorData;

extern SensorData sharedData;

void* background_sampler(void* arg);

#endif