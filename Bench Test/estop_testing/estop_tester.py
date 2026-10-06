import time
import json
import serial
import sys
import os

class EStopBenchTester:
    def __init__(self, port="COM8", baudrate=115200, timeout=1.0):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.ser = None
        self.event_log = []

    def connect(self):
        print(f"Connecting to ESP32 on {self.port} at {self.baudrate} baud...")
        self.ser = serial.Serial(self.port, self.baudrate, timeout=self.timeout)
        time.sleep(1.5)  # Allow boot/DTR toggle to settle
        # Flush incoming buffer and read any startup banners
        lines = self.read_all_lines()
        for l in lines:
            print(f"[ESP32 BOOT] {l}")
        return True

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()

    def send_cmd(self, cmd_str, wait_sec=0.2):
        if not self.ser or not self.ser.is_open:
            raise RuntimeError("Serial port not open")
        self.ser.reset_input_buffer()
        full_cmd = (cmd_str.strip() + "\n").encode('utf-8')
        self.ser.write(full_cmd)
        self.ser.flush()
        time.sleep(wait_sec)
        return self.read_all_lines()

    def read_all_lines(self):
        lines = []
        if not self.ser:
            return lines
        while self.ser.in_waiting > 0:
            try:
                line = self.ser.readline().decode('utf-8', errors='replace').strip()
                if line:
                    lines.append(line)
                    self.event_log.append({"time": time.time(), "line": line})
            except Exception as e:
                break
        return lines

    def get_status(self):
        lines = self.send_cmd("CMD:STATUS")
        status_info = {"raw_output": lines}
        for l in lines:
            if "[STATUS]" in l:
                # [STATUS] SAFETY=READY | RAW_PIN=0 | STABLE_PIN=0 | LATCHED=NO | UPTIME_MS=...
                parts = l.replace("[STATUS]", "").split("|")
                for p in parts:
                    if "=" in p:
                        k, v = p.split("=", 1)
                        status_info[k.strip().lower()] = v.strip()
        return status_info

    def send_motion_cmd(self):
        lines = self.send_cmd("CMD:MOVE_FORWARD")
        for l in lines:
            if "COMMAND = MOVE_FORWARD" in l:
                # e.g., COMMAND = MOVE_FORWARD | SAFETY = SAFE | RESULT = REJECTED
                return l
        return lines[0] if lines else "NO_RESPONSE"

    def send_reset(self):
        lines = self.send_cmd("CMD:RESET")
        for l in lines:
            if "[RESPONSE] RESET:" in l:
                return l
        return lines[0] if lines else "NO_RESPONSE"

    def monitor_transition(self, duration_sec=3.0):
        start = time.time()
        captured = []
        while time.time() - start < duration_sec:
            lines = self.read_all_lines()
            if lines:
                captured.extend(lines)
            time.sleep(0.05)
        return captured

if __name__ == "__main__":
    action = sys.argv[1] if len(sys.argv) > 1 else "status"
    tester = EStopBenchTester()
    tester.connect()
    
    if action == "status":
        print("Current Status:", json.dumps(tester.get_status(), indent=2))
    elif action == "motion":
        print("Motion Result:", tester.send_motion_cmd())
    elif action == "reset":
        print("Reset Result:", tester.send_reset())
    elif action == "monitor":
        dur = float(sys.argv[2]) if len(sys.argv) > 2 else 3.0
        print(f"Monitoring for {dur}s...")
        lines = tester.monitor_transition(dur)
        for l in lines:
            print(f"[EVENT] {l}")
        print("Status after monitor:", json.dumps(tester.get_status(), indent=2))
    
    tester.close()
