
#ifndef SPI_LIB_H
#define SPI_LIB_H

#include <stdint.h>


void spi_init(void);
uint16_t read_adc(uint8_t channel);
void spi_close(void);

#endif 