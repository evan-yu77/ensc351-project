#ifndef ENCODER_H
#define ENCODER_H

int encoder_init(int pinA, int pinB);

int encoder_get_rpm(void);

void encoder_cleanup(void);

#endif