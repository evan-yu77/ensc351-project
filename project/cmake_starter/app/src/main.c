// Main program to build the application
// Has main(); does initialization and cleanup and perhaps some basic logic.

#include <stdio.h>
#include <unistd.h>
#include "hal/spi_lib.h"

int main()
{
    printf("Initializing SPI...\n");
    spi_init();

    while(1) {
        uint16_t val_x = read_adc(0); // Read from channel 0
        uint16_t val_y = read_adc(1); // Read from channel 1

        printf("X-value: %4d  | Y-value: %4d\n", val_x, val_y);
        usleep(500000); // Sleep for 500ms
    }

    spi_close();
    return 0;
}