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

i2c_current_sensor.c / i2c_current_sensor.h

    Purpose: Interfaces with the INA219 current sensor over the Linux I2C bus (/dev/i2c-1, address 0x40).

    What it does: Configures and calibrates the INA219 for up to 1.0 A sensing, reads raw current/shunt/power registers, and converts the raw current reading into milliamps (mA).

server.py

    Purpose: Acts as the Python web bridge between the C background daemon and the browser dashboard.

    What it does: Runs a FastAPI/Uvicorn server on port 8080, polls the C daemon over UDP at 10 Hz, parses the telemetry string into JSON, and streams live updates to connected browsers via WebSockets (`/ws`) while forwarding dashboard commands back to UDP.

website/index.html & website/app.js

    Purpose: Provides the frontend web dashboard for real-time monitoring and motor control.

    What it does: Renders live Chart.js plots for vibration (Accel X/Y) and motor telemetry (Current mA & RPM), displays fault status and custom debugging tips, and sends speed, reset, and fault-toggle commands over WebSockets.

## How to use

### Prerequisites & hardware setup

It is crucial for the BeagleY-AI hardware to be properly configured before anything else is done:
-   **Device Trees:** Ensure the SPI overlay (`spidev0`) and PWM overlay are loaded in `/boot/firmware/extlinux/extlinux.conf`.
-   **Wiring:** Connect the MCP3208 ADC to SPI0, the INA219 to I2C-1, and the TB6612FNG motor driver to `pwmchipX` (replace X with your actual chip number) and the appropriate GPIO pins.
-   **Network:** The rest of the instructions assumes your host PC and BeagleY-AI are connected over local network, with the project directory shared via NFS.


### Compiling source code 

We use CMake in this project to cross compile from the host PC. To use this, you can run the following commands on the host:

```bash
cmake -S . -B build // create build folder that contains Makefile in order to compile project 

cd ~/ensc351/work/project/cmake_starter/build 


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
* reset - clears an active fault state and unlocks motor speed commands.
* fault on - enables automatic fault detection.
* fault off - disables automatic fault detection (and clears any currently active fault).

### Launching the web dashboard

Make sure the C daemon (proj351) is already running on the BeagleY-AI, then start the Python web bridge in the directory containing server.py and the website/ folder:

```bash 
python3 server.py
```
***(IMPORTANT: Requires `fastapi` and `uvicorn` installed in your Python environment.)***

Once running, open a web browser on your host PC and navigate to 

```plaintext
http://<BeagleY-AI-IP>:8080 
```

(Note: the BeagleY-AI IP address can be found in your network settings under your connected BeagleY-AI).

### Using the Web Dashboard & Fault Detection System

* ***Live Telemetry Charts:*** View real-time rolling graphs of Current (mA), RPM, and Vibration (Accel X / Accel Y) updated at 10 Hz.

* ***Motor Control:*** Drag the Speed slider (0–100%) to adjust the motor's PWM duty cycle in real time.

* ***Automatic Fault Detection & Safety Shutdown: The sampler continuously monitors*** sensor thresholds and will immediately halt the motor (`speed = 0`) if a fault occurs:

    * ***Code 1*** (`FAULT_STALL`): High current draw with low RPM while the motor is commanded to spin.

    * ***Code 2*** (`FAULT_OVERCURRENT`): Current spike exceeding the safe maximum limit.

    * ***Code 3*** (`FAULT_VIBRATION`): Accelerometer deviation exceeding the vibration threshold.

* ***Custom Debugging Tips***: When a fault triggers, the status banner turns red, the speed slider resets to 0%, and tailored hardware troubleshooting steps are printed to the BeagleY-AI terminal and displayed on the webpage.

* ***Resetting & Toggling Faults***: Click Reset Fault (or send reset via UDP) to clear a latched fault before restarting the motor. Use the Fault Detection Enabled checkbox (or `fault on` / `fault off`) to toggle protection on or off during testing.

### Shutting down 

To safely stop the background daemon and free the network port, run this command on the BeagleY-AI:

```bash
sudo killall proj351
```

If server.py is running in a terminal, press Ctrl + C to stop the web server.



