#include "hal/pwm_lib.h"
#include <stdio.h>
#include <unistd.h>

#define PWM_CHIP_PATH "/sys/class/pwm/pwmchip0"
#define PWM_PERIOD = 1000000 // 1 kHz frequency (1 ms period)

static void write_sysfs(const char* path, const char* value) {
    FILE* f = fopen(path, "w");
    if (f != NULL) {
        fputs(value, f);
        fclose(f);
    } else {
        printf("Error: Unable to write to %s\n", path);
    }

}

void pwm_init(void) {
    // Export PWM channel 0
    write_sysfs(PWM_CHIP_PATH "/export", "0");
    usleep(100000); // Wait for the sysfs entry to be created

    // Set the period
    write_sysfs(PWM_CHIP_PATH "/pwm0/period", "1000000"); // 1 kHz frequency

    // start with motor stopped 
    write_sysfs(PWM_CHIP_PATH "/pwm0/duty_cycle", "0");

    // Enable the PWM channel
    write_sysfs(PWM_CHIP_PATH "/pwm0/enable", "1");
}

void pwm_set_motor_speed(int speed) {
    if (speed < 0 || speed > 100) {
        printf("Error: Speed must be between 0 and 100\n");
        return;
    }

    // Calculate duty cycle based on speed percentage
    int duty_cycle = (speed * PWM_PERIOD) / 100;
    // Set the duty cycle
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", duty_cycle);
    write_sysfs(PWM_CHIP_PATH "/pwm0/duty_cycle", buffer);
}