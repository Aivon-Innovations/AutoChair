import serial
import time
import os

path = r"C:\Users\targa\.gemini\antigravity-ide\brain\c2475c72-c9fb-4a27-b77c-2ce60c5fbcb6\scratch\active_run_log.txt"

s = serial.Serial('COM8', 115200, timeout=1)
s.reset_input_buffer()

with open(path, 'w', encoding='utf-8', buffering=1) as f:
    f.write(f"# Active capture started at {time.time():.3f}\n")
    t0 = time.time()
    last_move_time = time.time()
    last_cnt = None
    stationary_count = 0
    # Capture for up to 30 seconds or until stopped for 6 seconds
    while time.time() - t0 < 30.0:
        line = s.readline()
        if line:
            dec = line.decode('utf-8', errors='replace').strip()
            if dec:
                f.write(f"{time.time():.3f} {dec}\n")
                if 'Encoder' in dec and 'R:' in dec:
                    # extract cnt
                    import re
                    m = re.search(r'R:\s+cnt=(-?\d+).*?idx=(\d+)', dec)
                    if m:
                        cnt = int(m.group(1))
                        idx = int(m.group(2))
                        print(f"cnt={cnt} idx={idx}")
                        if last_cnt is not None and cnt == last_cnt:
                            stationary_count += 1
                        else:
                            stationary_count = 0
                            last_cnt = cnt
                        if stationary_count >= 12: # 12 * 500ms = 6.0 seconds stationary
                            print("Stationary for 6 seconds, stopping capture.")
                            break
s.close()
