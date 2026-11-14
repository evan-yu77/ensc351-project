#!/usr/bin/env python3

# ============================================
# ENSC 351 MOTOR FAULT DETECTION SYSTEM
# BeagleY-AI + TB6612FNG + INA219
# ============================================

import gpiod
import subprocess
import time
import csv
from datetime import datetime


# ============================================
# CONFIGURATION
# ============================================

# Motor driver GPIO assignments

AIN1_CHIP = "gpiochip2"
AIN1_PIN = 8

AIN2_CHIP = "gpiochip1"
AIN2_PIN = 33

PWMA_CHIP = "gpiochip2"
PWMA_PIN = 16

STBY_CHIP = "gpiochip1"
STBY_PIN = 41


# INA219 settings

I2C_BUS = 1
INA219_ADDRESS = 0x40

# Your INA219 photo appears to show R010.
# R010 corresponds to 0.01 ohms.
# Confirm this before trusting current readings.

SHUNT_RESISTANCE = 0.01


# Motor test settings

TEST_DURATION = 3.0
SAMPLE_INTERVAL = 0.2


# Experimental thresholds
# NOT calibrated safety limits.

LOW_VOLTAGE = 2.0
HIGH_CURRENT = 500.0


# File used to save measurements

LOG_FILE = "motor_data.csv"


# ============================================
# INA219 SENSOR FUNCTIONS
# ============================================

def read_register(register):

    result = subprocess.check_output(
        [
            "i2cget",
            "-y",
            str(I2C_BUS),
            hex(INA219_ADDRESS),
            hex(register),
            "w"
        ],
        text=True,
        timeout=1
    ).strip()

    raw = int(result, 16)

    # Correct SMBus byte ordering

    swapped = ((raw & 0xFF) << 8) | (raw >> 8)

    return swapped


def read_voltage():

    raw = read_register(0x02)

    # INA219 bus voltage resolution: 4 mV

    voltage = (raw >> 3) * 0.004

    return voltage


def read_shunt_voltage():

    raw = read_register(0x01)

    # Signed 16-bit conversion

    if raw >= 32768:
        raw -= 65536

    # Each bit represents 10 microvolts

    voltage = raw * 0.00001

    return voltage


def read_current():

    shunt_voltage = read_shunt_voltage()

    # Ohm's law: I = V / R

    current_amps = shunt_voltage / SHUNT_RESISTANCE

    current_ma = current_amps * 1000

    return current_ma


def read_power(voltage, current_ma):

    current_amps = current_ma / 1000

    power = voltage * current_amps

    return power


# ============================================
# FAULT DETECTION
# ============================================

def detect_faults(voltage, current):

    faults = []

    if voltage < LOW_VOLTAGE:
        faults.append("LOW VOLTAGE")

    if abs(current) > HIGH_CURRENT:
        faults.append("HIGH CURRENT")

    if len(faults) == 0:
        return "NO THRESHOLD WARNING"

    return "WARNING: " + ", ".join(faults)


# ============================================
# GPIO CONFIGURATION
# ============================================

chip_a = None
chip_b = None

ain1 = None
ain2 = None
pwma = None
stby = None

requested = []


def setup_gpio():

    global chip_a, chip_b
    global ain1, ain2, pwma, stby

    chip_a = gpiod.Chip("gpiochip2")
    chip_b = gpiod.Chip("gpiochip1")

    ain1 = chip_a.get_line(AIN1_PIN)
    pwma = chip_a.get_line(PWMA_PIN)

    ain2 = chip_b.get_line(AIN2_PIN)
    stby = chip_b.get_line(STBY_PIN)

    lines = [ain1, ain2, pwma, stby]

    for line in lines:

        line.request(
            consumer="motor_monitor",
            type=gpiod.LINE_REQ_DIR_OUT,
            default_vals=[0]
        )

        requested.append(line)

    print("GPIO initialized successfully.")


# ============================================
# MOTOR CONTROL
# ============================================

def start_motor():

    # Forward direction

    ain1.set_value(1)
    ain2.set_value(0)

    # Full speed command

    pwma.set_value(1)

    # Enable motor driver

    stby.set_value(1)

    print("\nMOTOR ON\n")


def stop_motor():

    # Disable motor driver first

    if stby in requested:
        stby.set_value(0)

    if pwma in requested:
        pwma.set_value(0)

    if ain1 in requested:
        ain1.set_value(0)

    if ain2 in requested:
        ain2.set_value(0)


# ============================================
# MAIN MONITORING PROGRAM
# ============================================

def main():

    print("========================================")
    print(" ENSC 351 MOTOR FAULT DETECTION SYSTEM")
    print("========================================")

    print("\nChecking INA219...")

    # Check communication before enabling motor

    config = read_register(0x00)

    print(f"INA219 detected. Config: 0x{config:04X}")

    setup_gpio()

    voltage_data = []
    current_data = []
    power_data = []

    warning_count = 0

    print("\nStarting motor monitoring...")

    with open(LOG_FILE, "w", newline="") as file:

        writer = csv.writer(file)

        writer.writerow([
            "Timestamp",
            "Time (s)",
            "Voltage (V)",
            "Current (mA)",
            "Power (W)",
            "Status"
        ])

        start_motor()

        start_time = time.monotonic()

        while time.monotonic() - start_time < TEST_DURATION:

            voltage = read_voltage()

            current = read_current()

            power = read_power(voltage, current)

            elapsed = time.monotonic() - start_time

            status = detect_faults(voltage, current)

            voltage_data.append(voltage)
            current_data.append(current)
            power_data.append(power)

            if status.startswith("WARNING"):
                warning_count += 1

            print("----------------------------------------")
            print(f"Time:      {elapsed:.2f} seconds")
            print(f"Voltage:   {voltage:.3f} V")
            print(f"Current:   {current:.2f} mA")
            print(f"Power:     {power:.4f} W")
            print(f"Status:    {status}")

            writer.writerow([
                datetime.now().isoformat(),
                round(elapsed, 3),
                round(voltage, 3),
                round(current, 3),
                round(power, 4),
                status
            ])

            file.flush()

            remaining = (
                TEST_DURATION
                - (time.monotonic() - start_time)
            )

            if remaining > 0:
                time.sleep(min(SAMPLE_INTERVAL, remaining))

    # Stop motor before printing summary

    stop_motor()

    print("\nMOTOR OFF")

    print("\n========================================")
    print("           TEST SUMMARY")
    print("========================================")

    if voltage_data:

        avg_voltage = sum(voltage_data) / len(voltage_data)
        avg_current = sum(current_data) / len(current_data)
        avg_power = sum(power_data) / len(power_data)

        max_current = max(current_data)
        min_current = min(current_data)

        print(f"Samples:          {len(voltage_data)}")
        print(f"Average Voltage:  {avg_voltage:.3f} V")
        print(f"Average Current:  {avg_current:.2f} mA")
        print(f"Maximum Current:  {max_current:.2f} mA")
        print(f"Minimum Current:  {min_current:.2f} mA")
        print(f"Average Power:    {avg_power:.4f} W")
        print(f"Warning Samples:  {warning_count}")

    print(f"\nMeasurements saved to {LOG_FILE}")

    print("\nTEST COMPLETE")


# ============================================
# CLEANUP
# ============================================

if __name__ == "__main__":

    try:

        main()

    except KeyboardInterrupt:

        print("\nTest interrupted by user.")

    except Exception as error:

        print(f"\nERROR: {error}")

    finally:

        # Always attempt to disable motor

        try:
            stop_motor()
        except Exception:
            pass

        for line in requested:

            try:
                line.release()
            except Exception:
                pass

        if chip_a is not None:
            chip_a.close()

        if chip_b is not None:
            chip_b.close()

        print("\nMotor disabled. GPIO released.")
