#ifndef I2C_CURRENT_SENSOR_H
#define I2C_CURRENT_SENSOR_H

int i2c_init(void);

int i2c_current(int f);

int i2c_power(int f);

int i2c_cleanup(int f);


#endif