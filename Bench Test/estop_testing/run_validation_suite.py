import time
import json
import serial
import sys
import os

class AutoEstopValidator:
    def __init__(self, port="COM8", baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.log_file = "estop_validation_data.json"
        self.results = {
            "metadata": {
                "tester": "Antigravity Automated Test Harness",
                "operator": "Minaam",
                "board": "ESP32 WROOM-32",
                "gpio": 32,
                "pin_mode": "INPUT_PULLUP",
                "switch": "22 mm ProMax mushroom emergency-stop switch (NC terminals 11-12)",
                "date": time.strftime("%Y-%m-%d %H:%M:%S")
            },
            "basic_gpio_test": {},
            "command_rejection_tests": [],
            "safe_latch_tests": [],
            "repetition_cycles": [],
            "nc_fault_test": {},
            "summary": {}
        }
        self.load_results()

    def load_results(self):
        if os.path.exists(self.log_file):
            try:
                with open(self.log_file, "r") as f:
                    self.results = json.load(f)
            except Exception:
                pass

    def save_results(self):
        with open(self.log_file, "w") as f:
            json.dump(self.results, f, indent=2)

    def connect(self):
        if self.ser and self.ser.is_open:
            return
        self.ser = serial.Serial()
        self.ser.port = self.port
        self.ser.baudrate = self.baudrate
        self.ser.timeout = 1.0
        self.ser.dtr = False
        self.ser.rts = False
        self.ser.open()
        time.sleep(0.2)
        self.drain()

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            self.ser = None

    def drain(self):
        lines = []
        if not self.ser:
            return lines
        while self.ser.in_waiting > 0:
            try:
                line = self.ser.readline().decode('utf-8', errors='replace').strip()
                if line:
                    lines.append(line)
            except Exception:
                break
        return lines

    def send_cmd(self, cmd_str, wait_sec=0.2):
        self.connect()
        self.ser.reset_input_buffer()
        self.ser.write((cmd_str.strip() + "\n").encode('utf-8'))
        self.ser.flush()
        time.sleep(wait_sec)
        return self.drain()

    def get_status(self):
        lines = self.send_cmd("CMD:STATUS")
        status = {"raw_output": lines, "safety": "UNKNOWN", "raw_pin": -1, "stable_pin": -1, "latched": "UNKNOWN"}
        for l in lines:
            if "[STATUS]" in l:
                parts = l.replace("[STATUS]", "").split("|")
                for p in parts:
                    if "=" in p:
                        k, v = p.split("=", 1)
                        status[k.strip().lower()] = v.strip()
        return status

    def send_motion_cmd(self):
        lines = self.send_cmd("CMD:MOVE_FORWARD")
        res = {"raw": lines, "command": "MOVE_FORWARD", "safety": "UNKNOWN", "result": "UNKNOWN"}
        for l in lines:
            if "COMMAND = MOVE_FORWARD" in l:
                res["line"] = l
                parts = l.split("|")
                for p in parts:
                    if "=" in p:
                        k, v = p.split("=", 1)
                        res[k.strip().lower()] = v.strip()
        return res

    def send_reset_cmd(self):
        lines = self.send_cmd("CMD:RESET")
        res = {"raw": lines, "status": "UNKNOWN", "safety": "UNKNOWN"}
        for l in lines:
            if "[RESPONSE] RESET:" in l:
                res["line"] = l
                if "SUCCESS" in l:
                    res["status"] = "SUCCESS"
                    res["safety"] = "READY"
                else:
                    res["status"] = "REJECTED"
                    res["safety"] = "SAFE"
        return res

    def run_rejection_suite(self, count=5):
        """Run N command rejection tests while in SAFE state."""
        rejection_results = []
        for i in range(1, count + 1):
            st = self.get_status()
            mot = self.send_motion_cmd()
            passed = (st.get("safety") == "SAFE" and mot.get("result") == "REJECTED")
            record = {
                "test_num": i,
                "pre_status": st,
                "command_issued": "CMD:MOVE_FORWARD",
                "motion_response": mot,
                "passed": passed,
                "timestamp": time.time()
            }
            rejection_results.append(record)
            print(f"Rejection Test {i}/{count}: Command={mot.get('command')}, Safety={mot.get('safety')}, Result={mot.get('result')} -> {'PASS' if passed else 'FAIL'}")
            time.sleep(0.1)
        self.results["command_rejection_tests"] = rejection_results
        self.save_results()
        return rejection_results

    def record_latch_cycle(self, test_num, pre_release_status, post_release_status, reset_attempt_while_pressed=None, reset_response_after_release=None, final_status=None):
        passed = (
            post_release_status.get("raw_pin") == "0" and
            post_release_status.get("safety") == "SAFE" and
            post_release_status.get("latched") == "YES" and
            (final_status.get("safety") == "READY" if final_status else True)
        )
        record = {
            "test_num": test_num,
            "pre_release_status": pre_release_status,
            "post_release_status": post_release_status,
            "reset_while_pressed": reset_attempt_while_pressed,
            "reset_after_release": reset_response_after_release,
            "final_status": final_status,
            "passed": passed,
            "timestamp": time.time()
        }
        self.results["safe_latch_tests"].append(record)
        self.save_results()
        return record

    def record_repetition_cycle(self, cycle_num, pressed_status, released_status, reset_result, post_reset_status):
        passed = (
            pressed_status.get("raw_pin") == "1" and
            pressed_status.get("safety") == "SAFE" and
            released_status.get("raw_pin") == "0" and
            released_status.get("safety") == "SAFE" and
            reset_result.get("status") == "SUCCESS" and
            post_reset_status.get("safety") == "READY"
        )
        record = {
            "cycle_num": cycle_num,
            "pressed_status": pressed_status,
            "released_status": released_status,
            "reset_result": reset_result,
            "post_reset_status": post_reset_status,
            "passed": passed,
            "timestamp": time.time()
        }
        self.results["repetition_cycles"].append(record)
        self.save_results()
        return record

if __name__ == "__main__":
    v = AutoEstopValidator()
    print("Validator ready.")
