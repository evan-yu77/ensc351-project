#include "hal/i2c_current_sensor.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <stdint.h>

// ---------------------------------
//          THIS IS IMPORTANT
//-----------------------------------
//YOU HAVE TO SET THE MAXIMUM CURRENT THAT THE CHIP WILL SENSE, IT SETS UP THE CALLIBRATION FOR THE SENSOR
static const float max_expected_current = 1.0 ;
static const float shunt_resistance = 0.100;//this is on the INA219 current sensor

//THIS FILE ONLY WORKS WITH THE INA219 CURRENT SENSOR, it should most definately be changed if anyone uses anything else

//this is the path to the i2c i plan on using, it could be changed for a different i2c channel
#define I2C_PATH  "/dev/i2c-1"

//these are the adresses of/in the current sensor
static const uint8_t INA219_ADDRESS = 0x40;

static const uint8_t shunt = 0x01;
static const uint8_t power = 0x03; 
static const uint8_t current = 0x04;

int i2c_init(){

    //this is opening the device file for the i2c
    int f = open(I2C_PATH, O_RDWR);

    //making sure it worked
    if (f < 0){
        printf("could not open the i2c folder\n");
        
        return -1;
    }

    //this is selecting the address for the current sensor

    if (ioctl(f, I2C_SLAVE, INA219_ADDRESS)<0){
        printf("ioctl does not work\n");
        close(f);
        return -1;

    }
    //sets up the config
    i2c_config(f);
    //sets up the calibration
    i2c_calibration(f);

    return f;

}

//the 16 bit number to set up config, change this to change the settings
static const uint16_t config = 0x0097;

int i2c_config(int f){

    uint8_t data[3];

    data[0] = 0x00;
    //getting the first 
    data[1] = config >> 8;
    data[2] = config & 0xFF;

    if (write(f, data, 3) != 3){
        return -1;
    }

    return 1;


}


int i2c_calibration(int f){

    float  temp = max_expected_current/32768;

    uint16_t calibration_number = 0.04096 / (temp*shunt_resistance);

    uint8_t data[3];

    data[0] = 0x05; 
    data[1] = calibration_number >> 8;
    data[2] = calibration_number & 0xFF;

    if (write(f, data, 3) != 3){
        return -1;
    }

    return 1;

}

int i2c_power(int f){

    uint8_t buffer[2] = {0};

    //writing to the power terminal in the current sensor

    if (write(f, &power, 1) != 1){
        return -1;
    }
    
    if (read(f, buffer, sizeof(buffer)) != 2){
        return -1;
    }

    //combining the two 8 bit values into a 16 bit
    uint16_t result = ((uint16_t)buffer[0] << 8) | buffer[1];

    return result;
}    

//essentially the same as i2c_power

float i2c_current(int f){

    uint8_t buffer[2] = {0};

    
    
    if (write(f, &current, 1) != 1){
        return -1;
    }
    
    if (read(f, buffer, sizeof(buffer)) != 2){
        return -1;
    }

    //combining the two 8 bit values into a 16 bit
    int16_t result = (int16_t)(((uint16_t)buffer[0] << 8) | buffer[1]);

    return result;
}    

//this closes the device file
int i2c_cleanup(int f){

    close(f);

    //returns 1 for checking cleanup
    return 1;

}

int i2c_shunt_voltage(int f){

    uint8_t buffer[2] = {0};

    

    if (write(f, &shunt, 1) != 1){
        return -1;
    }
    
    if (read(f, buffer, sizeof(buffer)) != 2){
        return -1;
    }

    //combining the two 8 bit values into a 16 bit
    int16_t result = (int16_t)(((uint16_t)buffer[0] << 8) | buffer[1]);

    return result;
}    

