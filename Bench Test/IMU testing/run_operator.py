import serial
import time
import sys

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

def send_test_cmd(cmd, wait_sec=5.0):
    try:
        ser = serial.Serial('COM8', 115200, timeout=0.2)
    except Exception as e:
        print(f"Error opening COM8: {e}")
        return ""

    time.sleep(0.1)
    ser.reset_input_buffer()
    
    # Send command
    ser.write((cmd + "\n").encode('utf-8'))
    
    output = []
    start = time.time()
    while time.time() - start < wait_sec:
        line = ser.readline().decode('utf-8', errors='replace').rstrip()
        if line:
            output.append(line)
            print(line, flush=True)
            if "[COMMAND COMPLETE]" in line:
                break
    ser.close()
    return "\n".join(output)

if __name__ == '__main__':
    if len(sys.argv) > 1:
        cmd = sys.argv[1]
        wait = float(sys.argv[2]) if len(sys.argv) > 2 else 6.0
        send_test_cmd(cmd, wait)
    else:
        print("Usage: python run_operator.py <CMD> [WAIT_SEC]")
