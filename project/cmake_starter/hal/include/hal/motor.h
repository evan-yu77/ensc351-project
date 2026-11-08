#ifndef MOTOR_H
#define MOTOR_H

#include <stdbool.h>

void motor_init(void);
void motor_set_speed(int speed);
void motor_set_dir(bool forward);
void motor_cleanup(void);
void motor_stop(void);

#endif