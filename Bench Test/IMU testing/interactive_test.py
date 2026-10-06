import serial
import time
import sys

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

def run_interactive_step(cmd, test_name, action_instruction, record_sec=4.5):
    print("\n" + "="*70)
    print(f"  {test_name.upper()}")
    print("="*70)
    print(f"ACTION : {action_instruction}")
    print("SPEED  : Slow, smooth, controlled")
    print("-" * 70)
    print("Get ready...")
    for i in range(3, 0, -1):
        print(f"  Starting in {i}...", flush=True)
        time.sleep(1.0)
    
    print("\n>>> START MOVEMENT NOW! <<<", flush=True)
    
    try:
        ser = serial.Serial('COM8', 115200, timeout=0.2)
    except Exception as e:
        print(f"Error opening COM8: {e}")
        return

    time.sleep(0.1)
    ser.reset_input_buffer()
    ser.write((cmd + "\n").encode('utf-8'))
    
    start = time.time()
    while time.time() - start < record_sec:
        line = ser.readline().decode('utf-8', errors='replace').rstrip()
        if line:
            print(f"  {line}", flush=True)
            if "[COMMAND COMPLETE]" in line:
                break
    ser.close()
    
    print("\n>>> STOP MOVEMENT! <<<")
    print("Return the IMU to its stationary position and rest it on the table.")
    print("Holding stationary for 3 seconds...")
    time.sleep(3.0)
    print("="*70 + "\n")

if __name__ == '__main__':
    if len(sys.argv) > 3:
        cmd = sys.argv[1]
        name = sys.argv[2]
        action = sys.argv[3]
        run_interactive_step(cmd, name, action)
    else:
        print("Usage: python interactive_test.py <CMD> <NAME> <ACTION>")
