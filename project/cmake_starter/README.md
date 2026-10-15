# ENSC 351 FINAL PROJECT 

# copy and paste report since report already 
# explains everything 

# note: add SPI driver 

The machinery fault detection system will detect potential faults in a real-world, industrial mechanical assembly. Possible faults may include excessive noise and unusual changes in motion pattern (ex. interruption in motion). The BeagleY-AI board will serve as the core architecture/”brain” of the system, which relies on a C developed codebase that ensures the system is always active as long as it is connected to power. Vibration/motion data will be captured by the accelerometer, and audio/noise will be captured via an audio adapter and a microphone (3.5mm). When a possible problem is detected (e.g. changes in rotation pattern detected, noise exceeding threshold), a red LED light will light up as a warning indicator, and the fault will be time stamped and logged to the local SD card and then broadcast via a live dashboard (possibly a website). The type of error will also be displayed; this can be accomplished by implementing a library or custom I2C driver which will interact with a LCD that will display the type of error when the C codebase distinguishes/resolves the error type.


The MFDS will attach to the machinery it is troubleshooting at 3 points, The microphone will be attached to an audio adapter that is connected to the board, then the microphone will be attached/ held near the noise producing part of the machinery through the use of velcro or string. The accelerometer will be attached to the rotor/ blade/motor or any other point of movement with careful consideration of wires. A MOSFET will be built into the BYAI and motor circuit in order for safe connection from the BYAI to the motor. Then finally, the power/wires of the machinery will be connected to a short detector that is then attached to the board.


There will be no external network connections to the fault detection process itself, however we will integrate a Linux Debian operating system to develop and debug the system. 

External hardware needed:
3.5mm microphone or similar audio recording device 
Microphone will be supported by Linux ALSA (Advanced Linux Sound Architecture) audio drivers 
DC motor (5V or similar)
MOSFET or a similar logic-level controller that allows for safe connection from motor to BeagleY-AI board. 

