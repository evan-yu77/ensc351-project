# ENSC 351 FINAL PROJECT: THIS MANUAL IS IN PROGRESS

## Source code architecture and file manual

Below explains the purpose of all the source and HAL (hardware abstraction layer) files.

pwm_lib.c / pwm_lib.h

    Purpose: Handles low-level Linux sysfs operations for the BeagleY-AI's hardware PWM (Pulse Width Modulation).

    What it does: Exports the pwmchip, sets the 1 kHz period, and calculates the duty cycle to control power output without blocking the CPU.

motor.c / motor.h

    Purpose: Provides the high-level commands for the motor driver.

    What it does: Wraps pwm_lib.c to control the motor's speed, and uses standard Linux GPIO pins to toggle the IN1/IN2 direction pins for forward/reverse control.

spi_lib.c / spi_lib.h

    Purpose: Manages the SPI bus communication (/dev/spidev0.0).

    What it does: Configures the SPI clock speed and mode, and handles the low-level bit-shifting required to read raw analog values from the MCP3208 ADC (used for the accelerometer).

udp.c / udp.h

    Purpose: Runs the networking subsystem.

    What it does: Spawns an independent background thread that binds to UDP port 12345. It is responsible for parsing incoming network strings (like "speed 50"), updates the system state, and formats outgoing sensor data.

sampler.c / sampler.h

    Purpose: The main "brain" of the networking diagnostics tool. 

    What it does: Runs a precise 1000 Hz background thread. It polls the SPI sensors, reads the I2C current, updates the sharedData struct (so the UDP thread can see it), and continuously applies the requested motor speed to the hardware.

## How to use

### Prerequisites & hardware setup

It is crucial for the BeagleY-AI hardware to be properly configured before anything else is done:
*   **Device Trees:** Ensure the SPI overlay (`spidev0`) and PWM overlay are loaded in `/boot/firmware/extlinux/extlinux.conf`.
*   **Wiring:** Connect the MCP3208 ADC to SPI0, the INA219 to I2C-1, and the TB6612FNG motor driver to `pwmchip3` and GPIOs 517/518. 
*   **Network:** The project assumes your host PC and BeagleY-AI are connected over local network, with the project directory shared via NFS.


### Compiling source code 

We use CMake in this project to cross compile from the host PC. To use this, you can run the following commands on the host:

```bash
cmake -S . -B build // create build folder that contains Makefile in order to compile project 

cd ~/ensc351/work/as1/cmake_starter/build 


// pass compiler paths to cmd line to avoid exec format errors while running on beagleboard (run from build folder)

cmake -DCMAKE_SYSTEM_NAME=Linux \
      -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
      -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
      -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
      ..
```

### Launching the daemon (background sampler and UDP listener):
Access the BeagleY-AI using SSH. Make sure to mount the NFS directory and run the following with superuser privileges:

```bash 
cd /mnt/remote/build
sudo ./proj351 &
```

This will initialize hardware pins and start listening for commands on UDP port 12345. 

### Send commands via UDP 

To interact with the background daemon, use netcat. After going through the previous steps, open a terminal on the host and open the UDP connection with the following commands:

```bash 
netcat -u 127.0.0.1 12345 // IMPORTANT: Change the 127.0.0.1 IP address with the BeagleY-AI's actual IP address if you decide to run this remotely.
```

Once connected, the following commands can be used:

* speed 50 - sets motor to 50% PWM duty cycle. (accepts 0-100)
* ? - Server will reply with the real time sensor data string. It will have the format: Accel X: 4, Accel Y: 1007, Current (mA): 0, RPM: 0, Speed: 50%


### Shutting down 

To safely stop the background daemon and free the network port, run this command on the BeagleY-AI:

```bash
sudo killall proj351
```

