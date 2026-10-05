import serial
import time
import sys

PORT = "/dev/cu.usbserial-0001"
BAUD = 115200

def send_and_collect(cmd, duration=10):
    print(f"Connecting to {PORT} at {BAUD}...")
    with serial.Serial(PORT, BAUD, timeout=1) as ser:
        time.sleep(2)  # Wait for ESP32 boot
        ser.reset_input_buffer()
        print(f"Sending command: {cmd}")
        ser.write((cmd + "\n").encode())
        
        start_time = time.time()
        lines = []
        while time.time() - start_time < duration:
            line = ser.readline().decode(errors="ignore").strip()
            if line:
                print(f"[ESP32] {line}")
                lines.append(line)
        return lines

if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "CMD:STATUS"
    dur = int(sys.argv[2]) if len(sys.argv) > 2 else 10
    send_and_collect(cmd, dur)
