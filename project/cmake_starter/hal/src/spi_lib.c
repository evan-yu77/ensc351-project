#include "hal/spi_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define SPI_DEV "/dev/spidev0.0"


static int spi_fd;

void spi_init(void) {
    spi_fd = open(SPI_DEV, O_RDWR);
    if (spi_fd < 0) {
        perror("Failed to open SPI device");
        exit(EXIT_FAILURE);
    }

    uint8_t mode = SPI_MODE_0;
    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0) {
        perror("Failed to set SPI mode");
        close(spi_fd);
        exit(EXIT_FAILURE);
    }
}

uint16_t read_adc(uint8_t channel) {
    if (channel > 7) {
        fprintf(stderr, "Invalid ADC channel: %d\n", channel);
        return 0;
    }

    uint8_t tx[] = { 0x06 | (channel >> 2), (channel & 0x03) << 6, 0x00 };
    uint8_t rx[3] = { 0, 0, 0 };

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = 3,
        .speed_hz = 1000000,
        .bits_per_word = 8,
    };

    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        perror("Failed to read from ADC");
        return 0;
    }

    return ((rx[1] & 0x03) << 8) | rx[2];
}

void spi_close(void) {
    close(spi_fd);
}