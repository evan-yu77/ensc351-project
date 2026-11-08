#include "hal/motor.h"
#include "hal/pwm_lib.h"
#include <stdio.h>
#include <unistd.h>

// replace this with the actual GPIO pin numbers for motor driver later
#define IN1_PIN "517"
#define IN2_PIN "518"

static void write_gpio(const char* pin, const char* value) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%s/value", pin);
    FILE* f = fopen(path, "w");
    if (f != NULL) {
        fputs(value, f);
        fclose(f);
    } else {
        printf("Error: Unable to write to %s\n", path);
    }
}


static void export_and_set_out(const char* pin) {
    char path[64];
    FILE* f = fopen("/sys/class/gpio/export", "w");
    if (f != NULL) {
        fputs(pin, f);
        fclose(f);
    } else {
        printf("Error: Unable to export GPIO %s\n", pin);
        return;
    }

    usleep(50000); // Wait for the sysfs entry to be created

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%s/direction", pin);
    f = fopen(path, "w");
    if (f != NULL) {
        fputs("out", f);
        fclose(f);
    } else {
        printf("Error: Unable to set direction for GPIO %s\n", pin);
    }
}

void motor_init(void) {
    pwm_init(); // Initialize PWM for motor speed control

    export_and_set_out(IN1_PIN);
    export_and_set_out(IN2_PIN);

    motor_stop(); // Ensure motor is stopped on initialization
}

void motor_set_dir(bool forward) {
    if (forward) {
        write_gpio(IN1_PIN, "1");
        write_gpio(IN2_PIN, "0");
    } else {
        write_gpio(IN1_PIN, "0");
        write_gpio(IN2_PIN, "1");
    }
}

void motor_set_speed(int speed) {
    pwm_set_speed(speed);
}

void motor_stop(void) {
    write_gpio(IN1_PIN, "0");
    write_gpio(IN2_PIN, "0");
    pwm_set_speed(0); // Stop PWM signal
}

