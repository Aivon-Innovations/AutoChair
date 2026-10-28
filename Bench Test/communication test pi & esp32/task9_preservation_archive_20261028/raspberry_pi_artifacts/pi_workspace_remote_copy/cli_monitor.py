#!/usr/bin/env python3
"""
AutoChair Raspberry Pi ↔ ESP32 Standalone Communication CLI & Test Monitor.
Allows manual interactive testing or running automated test scenarios.
"""

import sys
import os
import time
import argparse
import json
import logging

# Ensure parent and current directory are on sys.path for direct or module launch
_cur_dir = os.path.dirname(os.path.abspath(__file__))
_parent_dir = os.path.abspath(os.path.join(_cur_dir, ".."))
if _cur_dir not in sys.path:
    sys.path.insert(0, _cur_dir)
if _parent_dir not in sys.path:
    sys.path.insert(0, _parent_dir)

try:
    from .client import PiEsp32SerialClient
    from .protocol import State, Mode
except (ImportError, ValueError):
    try:
        from raspberry_pi.client import PiEsp32SerialClient
        from raspberry_pi.protocol import State, Mode
    except ImportError:
        from client import PiEsp32SerialClient
        from protocol import State, Mode

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")

def print_telemetry(t: dict):
    print("\n--- [ESP32 TELEMETRY] ---")
    print(f"  State: {t.get('state')} | Mode: {t.get('mode')} | Uptime: {t.get('uptime_ms')} ms | Seq: {t.get('telemetry_seq')}")
    hb = t.get('heartbeat', {})
    print(f"  Heartbeat Health: OK={hb.get('ok')} | Age={hb.get('last_received_age_ms')} ms | Missed={hb.get('missed_count')}")
    sensors = t.get('sensors', {})
    print(f"  Sensors: Ultrasonic={sensors.get('ultrasonic')}, IMU={sensors.get('imu')}, Encoders={sensors.get('encoders')}")
    print(f"  Simulated Motion: {t.get('simulated_motion')}")
    print("-------------------------\n")

def interactive_loop(client: PiEsp32SerialClient):

    print("\n=======================================================")
    print(" AutoChair Pi ↔ ESP32 Standalone Serial Monitor")
    print(" Connected Port:", client.port)
    print("=======================================================")
    print(" Commands:")
    print("  1: PING")
    print("  2: GET_STATUS")
    print("  3: SET_MODE (MANUAL / ASSISTED / NONE)")
    print("  4: MOVE (TEST_FORWARD, TEST_LEFT, TEST_RIGHT, STOP)")
    print("  5: RESET (Safety Recovery / Arming to READY)")
    print("  6: Toggle Heartbeat TX (Simulate Pi Hang / Timeout)")
    print("  7: Send Malformed JSON (Robustness Check)")
    print("  8: Send Unknown Command (Rejection Check)")
    print("  q: Quit")
    print("=======================================================\n")

    while client.is_connected:
        try:
            choice = input("Enter command [1-8, q]: ").strip()
            if not choice:
                continue
            if choice == 'q':
                break

            if choice == '1':
                res = client.ping()
                print("PING Response:", json.dumps(res, indent=2))
            elif choice == '2':
                res = client.get_status()
                print("STATUS Response:", json.dumps(res, indent=2))
            elif choice == '3':
                m = input("Enter mode [MANUAL, ASSISTED, NONE]: ").strip().upper()
                res = client.set_mode(m)
                print("SET_MODE Response:", json.dumps(res, indent=2))
            elif choice == '4':
                act = input("Enter motion action [TEST_FORWARD, TEST_LEFT, TEST_RIGHT, STOP]: ").strip().upper()
                spd = int(input("Enter speed % (0-100) [default 50]: ") or "50")
                res = client.send_motion(act, spd)
                print("MOVE Response:", json.dumps(res, indent=2))
            elif choice == '5':
                res = client.reset_safety()
                print("RESET Response:", json.dumps(res, indent=2))
            elif choice == '6':
                client.enable_heartbeat = not client.enable_heartbeat
                state_str = "ENABLED" if client.enable_heartbeat else "DISABLED (Simulating Timeout!)"
                print(f"Heartbeat TX is now: {state_str}")
            elif choice == '7':
                print("Sending raw malformed JSON line: '{\"broken_json: 123'")
                if client._ser and client._ser.is_open:
                    client._ser.write(b'{"broken_json: 123\n')
                    client._ser.flush()
                print("Sent. Waiting 1 second to observe telemetry / response...")
                time.sleep(1.0)
            elif choice == '8':
                res = client.send_command("SELF_DESTRUCT_NOW", {"foo": "bar"})
                print("Unknown Command Response (Expected REJECTED):", json.dumps(res, indent=2))
            else:
                print("Unknown selection.")
        except (KeyboardInterrupt, EOFError):
            break

    print("\nExiting monitor...")

def main():
    parser = argparse.ArgumentParser(description="AutoChair Standalone Pi <-> ESP32 Serial Monitor")
    parser.add_argument("--port", type=str, required=True, help="Serial port (e.g. /dev/ttyUSB0, COM8, /dev/ttyACM0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--no-hb", action="store_true", help="Start with background heartbeats disabled")
    args = parser.parse_args()

    client = PiEsp32SerialClient(
        port=args.port,
        baudrate=args.baud,
        enable_heartbeat=not args.no_hb
    )

    if not client.connect():
        print(f"Failed to connect to {args.port}")
        sys.exit(1)

    try:
        interactive_loop(client)
    finally:
        client.disconnect()

if __name__ == "__main__":
    main()
