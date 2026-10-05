import glob
import sys
import time
import serial

DEFAULT_PORTS = ["/dev/cu.SLAB_USBtoUART", "/dev/cu.usbserial-0001"]
BAUD = 115200

def find_serial_port():
    for p in DEFAULT_PORTS:
        if glob.glob(p):
            return p
    # Fallback to any USB serial port
    matches = glob.glob("/dev/cu.usb*") + glob.glob("/dev/cu.SLAB*")
    return matches[0] if matches else "/dev/cu.SLAB_USBtoUART"

def run_telemetry_capture(cmd=None, duration=15, port=None):
    port = port or find_serial_port()
    print(f"Connecting to {port} at {BAUD} baud...")

    try:
        with serial.Serial(port, BAUD, timeout=1) as ser:
            time.sleep(2)  # Wait for ESP32 UART connection to stabilize
            ser.reset_input_buffer()

            if cmd:
                print(f"Sending mode command: {cmd}")
                ser.write((cmd + "\n").encode())

            print(f"Streaming Ultrasonic telemetry for {duration} seconds (Ctrl+C to stop)...\n")
            start_time = time.time()
            lines = []
            while time.time() - start_time < duration:
                line = ser.readline().decode(errors="ignore").strip()
                if line:
                    print(line)
                    lines.append(line)
            return lines
    except Exception as e:
        print(f"Serial Error: {e}")
        return []

if __name__ == "__main__":
    dur = 15
    cmd = None

    if len(sys.argv) > 1:
        # Check if first arg is an integer duration or a command
        if sys.argv[1].isdigit():
            dur = int(sys.argv[1])
            if len(sys.argv) > 2:
                cmd = sys.argv[2]
        else:
            cmd = sys.argv[1]
            if len(sys.argv) > 2 and sys.argv[2].isdigit():
                dur = int(sys.argv[2])

    run_telemetry_capture(cmd, dur)
