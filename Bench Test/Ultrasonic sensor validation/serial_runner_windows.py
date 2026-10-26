"""
serial_runner_windows.py
AutoChair — ESP32 Serial Telemetry Capture (Windows)

Usage:
    python serial_runner_windows.py                    # Auto-detect COM port, 30s capture
    python serial_runner_windows.py --port COM4        # Specify port
    python serial_runner_windows.py --duration 60      # Capture for 60 seconds
    python serial_runner_windows.py --port COM4 --duration 60 --out telemetry.log

Requirements:
    pip install pyserial

Notes:
    - Run this on the Windows laptop connected to the ESP32 via USB.
    - Baud rate matches firmware: 115200.
    - Output is saved to a timestamped log file by default.
    - Each line is written with a local timestamp prefix for analysis.
"""

import argparse
import datetime
import glob
import os
import sys
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("ERROR: pyserial not installed. Run: pip install pyserial")
    sys.exit(1)

BAUD = 115200
DEFAULT_DURATION = 30  # seconds


# ---------------------------------------------------------------------------
# Port discovery (Windows: COM ports)
# ---------------------------------------------------------------------------

def list_com_ports():
    """List all available COM ports on Windows."""
    ports = list(serial.tools.list_ports.comports())
    return ports


def find_esp32_port():
    """
    Attempt to auto-detect the ESP32 COM port.
    Looks for CP210x, CH340, or FTDI USB-UART bridge descriptors.
    Returns the port name string, or None if not found.
    """
    esp32_keywords = [
        "CP210", "CH340", "CH341", "FTDI", "USB Serial",
        "USB-SERIAL", "Silicon Labs", "USB UART"
    ]
    ports = list_com_ports()
    for p in ports:
        desc = (p.description or "").upper()
        mfg  = (p.manufacturer or "").upper()
        for kw in esp32_keywords:
            if kw.upper() in desc or kw.upper() in mfg:
                return p.device
    # Fallback: return the first available COM port
    if ports:
        return ports[0].device
    return None


# ---------------------------------------------------------------------------
# Telemetry capture
# ---------------------------------------------------------------------------

def run_capture(port: str, duration: int, out_file: str):
    print(f"\n{'='*60}")
    print(f"AutoChair — ESP32 Serial Telemetry Capture")
    print(f"{'='*60}")
    print(f"  Port    : {port}")
    print(f"  Baud    : {BAUD}")
    print(f"  Duration: {duration} s")
    print(f"  Output  : {out_file}")
    print(f"{'='*60}\n")

    line_count = 0
    start_time = time.time()

    try:
        ser = serial.Serial(port, BAUD, timeout=1)
    except serial.SerialException as e:
        print(f"ERROR: Could not open {port}: {e}")
        print("\nAvailable ports:")
        for p in list_com_ports():
            print(f"  {p.device}  —  {p.description}")
        sys.exit(1)

    print(f"Connected. Waiting 2 s for ESP32 UART to stabilise...\n")
    time.sleep(2)
    ser.reset_input_buffer()

    with open(out_file, "w", encoding="utf-8") as f:
        # Write header
        f.write(f"# AutoChair ESP32 Serial Capture\n")
        f.write(f"# Port: {port}  Baud: {BAUD}\n")
        f.write(f"# Started: {datetime.datetime.now().isoformat()}\n")
        f.write(f"# Duration: {duration} s\n")
        f.write("#\n")

        elapsed = 0.0
        while elapsed < duration:
            try:
                raw = ser.readline()
                if raw:
                    ts = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
                    line = raw.decode("utf-8", errors="replace").rstrip()
                    output = f"[{ts}] {line}"
                    print(output)
                    f.write(output + "\n")
                    f.flush()
                    line_count += 1
            except serial.SerialException as e:
                print(f"\nERROR: Serial read failed: {e}")
                break
            except KeyboardInterrupt:
                print("\nCapture interrupted by user.")
                break
            elapsed = time.time() - start_time

    ser.close()

    elapsed_actual = time.time() - start_time
    print(f"\n{'='*60}")
    print(f"Capture complete.")
    print(f"  Lines captured : {line_count}")
    print(f"  Elapsed        : {elapsed_actual:.1f} s")
    print(f"  Output file    : {os.path.abspath(out_file)}")
    print(f"{'='*60}\n")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="AutoChair ESP32 serial telemetry capture (Windows)"
    )
    parser.add_argument(
        "--port", "-p",
        default=None,
        help="COM port (e.g. COM3, COM4). Auto-detected if omitted."
    )
    parser.add_argument(
        "--duration", "-d",
        type=int,
        default=DEFAULT_DURATION,
        help=f"Capture duration in seconds (default: {DEFAULT_DURATION})"
    )
    parser.add_argument(
        "--out", "-o",
        default=None,
        help="Output log file path. Auto-named with timestamp if omitted."
    )
    parser.add_argument(
        "--list-ports", "-l",
        action="store_true",
        help="List available COM ports and exit."
    )
    args = parser.parse_args()

    if args.list_ports:
        print("Available COM ports:")
        ports = list_com_ports()
        if not ports:
            print("  (none found)")
        for p in ports:
            print(f"  {p.device:<10} {p.description}")
        sys.exit(0)

    port = args.port
    if port is None:
        port = find_esp32_port()
        if port is None:
            print("ERROR: No COM port found. Connect the ESP32 and try again,")
            print("       or specify the port with --port COM<N>.")
            print("\nAvailable ports:")
            for p in list_com_ports():
                print(f"  {p.device}  —  {p.description}")
            sys.exit(1)
        print(f"Auto-detected port: {port}")

    out_file = args.out
    if out_file is None:
        ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        out_file = f"autochair_telemetry_{ts}.log"

    run_capture(port=port, duration=args.duration, out_file=out_file)


if __name__ == "__main__":
    main()
