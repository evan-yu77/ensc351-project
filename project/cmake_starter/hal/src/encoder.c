#include "hal/encoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>
#include <string.h>
#include <time.h>


#define PULSES_PER_REV 210.0 

static int fd_A = -1;
static int fd_B = -1;
static  int pinA_sysfs = 0;
static int pinB_sysfs = 0;

static volatile long pulse_cnt = 0;
static volatile int is_running = 0;
static pthread_t encoder_thread;
static struct timespec last_time;

static void write_sysfs(const char* path, const char* value) {
    FILE* f = fopen(path, "w");
    if (f) {
        fputs(value, f);
        fclose(f);
    } else {
        perror("Failed to write to sysfs");
    }

}

// background thread to catch hardware edge interrupts 
static void* encoder_worker(void* arg) {

    (void)arg; // Unused parameter
    struct pollfd pfd;
    pfd.fd = fd_A;
    pfd.events = POLLPRI | POLLERR;
    char buffer[4];


    while (is_running) {
        int ret = poll(&pfd, 1, 10); 
        if (ret > 0 && (pfd.revents & POLLPRI)) {
            if (pfd.revents & POLLPRI) {
                lseek(fd_A, 0, SEEK_SET);
                read(fd_A, buffer, sizeof(buffer));

                lseek(fd_B, 0, SEEK_SET);
                if (read(fd_B, buffer, sizeof(buffer)) > 0) {
                    if (buffer[0] == '1') {
                        pulse_cnt++;
                    } else {
                        pulse_cnt--;
                    }
                }
            }

        }
    }

    return NULL;
}

int encoder_init(int pinA, int pinB) {
    pinA_sysfs = pinA;
    pinB_sysfs = pinB;

    char path[128], pinA_str[16], pinB_str[16];

    sprintf(pinA_str, "%d", pinA);
    sprintf(pinB_str, "%d", pinB);

    write_sysfs("/sys/class/gpio/export", pinA_str);
    write_sysfs("/sys/class/gpio/export", pinB_str);
    usleep(100000); // Wait for sysfs to create the files

    sprintf(path, "/sys/class/gpio/gpio%s/direction", pinA_str);
    write_sysfs(path, "in");

    sprintf(path, "/sys/class/gpio/gpio%s/direction", pinB_str);
    write_sysfs(path, "in");
    
    sprintf(path, "/sys/class/gpio/gpio%s/edge", pinA_str);
    write_sysfs(path, "rising");

    sprintf(path, "/sys/class/gpio/gpio%s/value", pinA_str);
    fd_A = open(path, O_RDONLY | O_NONBLOCK);

    sprintf(path, "/sys/class/gpio/gpio%s/value", pinB_str);
    fd_B = open(path, O_RDONLY | O_NONBLOCK);

    clock_gettime(CLOCK_MONOTONIC, &last_time);
    is_running = 1;
    pthread_create(&encoder_thread, NULL, encoder_worker, NULL);


    return 0;
}

int encoder_get_rpm(void) {
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    double elapsed_time = (current_time.tv_sec - last_time.tv_sec) + 
                          (current_time.tv_nsec - last_time.tv_nsec) / 1e9;

    long pulses = pulse_cnt;
    pulse_cnt = 0; // reset pulse count for next measurement
    last_time = current_time;

    if (elapsed_time > 0.0) {
        double revolutions = pulses / PULSES_PER_REV;
        double rpm = (revolutions / elapsed_time) * 60.0;

        return (int)rpm;
    }

    return 0;
}

void encoder_cleanup(void) {
    is_running = 0;
    pthread_join(encoder_thread, NULL);

    if (fd_A >= 0) {
        close(fd_A);
    }

    if (fd_B >= 0) {
        close(fd_B);
    }

    char pinA_str[16], pinB_str[16];
    sprintf(pinA_str, "%d", pinA_sysfs);
    sprintf(pinB_str, "%d", pinB_sysfs);
    write_sysfs("/sys/class/gpio/unexport", pinA_str);
    write_sysfs("/sys/class/gpio/unexport", pinB_str);
}

