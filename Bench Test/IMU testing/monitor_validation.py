import serial
import time
import sys

def run_monitor():
    try:
        ser = serial.Serial('COM8', 115200, timeout=1.0)
    except Exception as e:
        print(f"Error opening COM8: {e}")
        return

    # Trigger ESP32 hardware reset via DTR/RTS
    ser.dtr = False
    ser.rts = True
    time.sleep(0.1)
    ser.rts = False
    time.sleep(0.2)

    print("=== SERIAL MONITOR OPENED ON COM8 (115200 baud) ===")
    
    start_time = time.time()
    # Monitor for up to 120 seconds or until sequence completes
    while time.time() - start_time < 120:
        line = ser.readline().decode('utf-8', errors='replace').rstrip()
        if line:
            print(line, flush=True)
            if "Bench validation sequence finished" in line:
                break
            if "Skipping subsequent sensor tests" in line:
                # Wait for final report
                time.sleep(1)
                while ser.in_waiting:
                    print(ser.readline().decode('utf-8', errors='replace').rstrip(), flush=True)
                break

    ser.close()
    print("=== MONITOR FINISHED ===")

if __name__ == '__main__':
    run_monitor()
