#!/usr/bin/env python3
"""
Serial-to-Keyboard Bridge for CV Axle Controller

Reads movement commands from the Arduino Uno over serial and
simulates keyboard presses on the host computer.

This bridge is a temporary prototype tool — when the Pro Micro
arrives, the Arduino will send keystrokes directly via USB HID
and this script will no longer be needed.

Requirements:
    pip install pyserial pynput

Usage:
    python serial_keyboard_bridge.py [--port /dev/cu.usbmodem*] [--baud 115200]

On macOS, the port is typically /dev/cu.usbmodemXXXX or /dev/cu.usbserialXXXX.
The script will auto-detect if no port is specified.
"""

import argparse
import sys
import time
import glob
import serial
from pynput.keyboard import Controller, Key

# === Default Key Mappings ===
# These define what keystrokes each joystick command sends.
# Modify these to match whatever the measurement software expects.
KEY_MAP = {
    # Joystick 1 (Line 1 - left axle)
    "JOY1_LEFT_COARSE":  "a",
    "JOY1_RIGHT_COARSE": "d",
    "JOY1_LEFT_FINE":    "q",
    "JOY1_RIGHT_FINE":   "e",
    "JOY1_BTN":          "1",

    # Joystick 2 (Line 2 - right axle)
    "JOY2_LEFT_COARSE":  "j",
    "JOY2_RIGHT_COARSE": "l",
    "JOY2_LEFT_FINE":    "u",
    "JOY2_RIGHT_FINE":   "o",
    "JOY2_BTN":          "2",
}


def find_arduino_port():
    """Auto-detect Arduino serial port on macOS/Linux."""
    patterns = [
        "/dev/cu.usbmodem*",
        "/dev/cu.usbserial*",
        "/dev/ttyUSB*",
        "/dev/ttyACM*",
    ]
    for pattern in patterns:
        ports = glob.glob(pattern)
        if ports:
            return ports[0]
    return None


def main():
    parser = argparse.ArgumentParser(description="CV Axle Controller Serial-to-Keyboard Bridge")
    parser.add_argument("--port", help="Serial port (auto-detected if not specified)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--list-keys", action="store_true", help="Show current key mappings and exit")
    parser.add_argument("--dry-run", action="store_true", help="Print commands without sending keystrokes")
    args = parser.parse_args()

    if args.list_keys:
        print("\nCurrent Key Mappings:")
        print("-" * 40)
        for cmd, key in KEY_MAP.items():
            print(f"  {cmd:25s} → '{key}'")
        print()
        sys.exit(0)

    # Find serial port
    port = args.port or find_arduino_port()
    if not port:
        print("ERROR: No Arduino found. Specify port with --port")
        print("  On macOS: ls /dev/cu.usb*")
        print("  On Linux: ls /dev/ttyUSB* /dev/ttyACM*")
        sys.exit(1)

    # Initialize keyboard controller
    keyboard = Controller()

    print(f"Connecting to {port} at {args.baud} baud...")

    try:
        ser = serial.Serial(port, args.baud, timeout=1)
        time.sleep(2)  # Wait for Arduino reset after serial connection
        print("Connected. Waiting for controller...")

        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode("utf-8", errors="ignore").strip()

                if not line:
                    continue

                if line == "AXLE_CONTROLLER_READY":
                    print("Controller ready!")
                    print(f"Mode: {'DRY RUN' if args.dry_run else 'LIVE'}")
                    print("Press Ctrl+C to quit.\n")
                    continue

                # Look up the command in our key map
                if line in KEY_MAP:
                    key = KEY_MAP[line]
                    if args.dry_run:
                        print(f"  [{line}] → would press '{key}'")
                    else:
                        keyboard.press(key)
                        keyboard.release(key)
                        print(f"  [{line}] → '{key}'")
                else:
                    # Unknown command — print for debugging
                    print(f"  [UNKNOWN] {line}")

    except serial.SerialException as e:
        print(f"Serial error: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n\nBridge stopped.")
        ser.close()


if __name__ == "__main__":
    main()
