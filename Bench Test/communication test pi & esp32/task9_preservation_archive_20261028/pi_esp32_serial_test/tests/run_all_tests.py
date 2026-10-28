#!/usr/bin/env python3
"""
Master Test Runner for AutoChair Standalone Serial Communication Suite (Stage A.3 Final Verification).
Runs native C++ firmware verification tests and all Python unit/integration tests.
Extracts real execution counts dynamically from runner outputs.
Saves log output to logs/stage_a3_final_verification.log.
"""

import unittest
import sys
import os
import time
import subprocess
import re

def run_suite():
    test_dir = os.path.dirname(os.path.abspath(__file__))
    proj_dir = os.path.abspath(os.path.join(test_dir, ".."))
    log_dir = os.path.join(proj_dir, "logs")
    os.makedirs(log_dir, exist_ok=True)
    log_file_path = os.path.join(log_dir, "stage_a3_final_verification.log")

    output_lines = []

    def log(msg):
        print(msg)
        output_lines.append(msg)

    log("\n=======================================================")
    log(" Running AutoChair Standalone Communication Test Suite")
    log(" (Stage A.3 Final Verification)")
    log("=======================================================\n")

    # 1. Native C++ State Machine & Protocol Tests
    log("[STEP 1] Compiling and running Native C++ Firmware State Machine & Protocol Tests...")
    cpp_bin = os.path.join(test_dir, "test_cpp_state_machine")
    compile_cmd = [
        "clang++", "-std=c++17", "-Wall", "-Wextra",
        f"-I{test_dir}",
        f"-I{os.path.join(proj_dir, 'esp32_firmware', 'src')}",
        f"-I{os.path.join(proj_dir, 'esp32_firmware', '.pio', 'libdeps', 'esp32dev', 'ArduinoJson', 'src')}",
        os.path.join(test_dir, "test_cpp_state_machine.cpp"),
        os.path.join(proj_dir, "esp32_firmware", "src", "protocol.cpp"),
        os.path.join(proj_dir, "esp32_firmware", "src", "state_machine.cpp"),
        "-o", cpp_bin
    ]

    comp_res = subprocess.run(compile_cmd, capture_output=True, text=True)
    if comp_res.returncode != 0:
        log(f"C++ Compilation Failed:\n{comp_res.stderr}")
        return 1

    run_cpp_res = subprocess.run([cpp_bin], capture_output=True, text=True)
    cpp_output = run_cpp_res.stdout.strip()
    log(cpp_output)
    if run_cpp_res.returncode != 0:
        log(f"C++ Test Execution Failed:\n{run_cpp_res.stderr}")
        return 1

    # Extract dynamic count from C++ test output
    cpp_checks_match = re.search(r"All\s+(\d+)\s+native C\+\+ assertion checks PASSED", cpp_output)
    cpp_checks_count = int(cpp_checks_match.group(1)) if cpp_checks_match else "Unknown"

    log("\n[STEP 2] Running Python Protocol & Integration Test Suite...")
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    suite.addTests(loader.discover(start_dir=test_dir, pattern="test_*.py"))

    protocol_suite = loader.loadTestsFromName("test_protocol")
    state_machine_suite = loader.loadTestsFromName("test_state_machine_logic")
    client_suite = loader.loadTestsFromName("test_client_harness")

    runner = unittest.TextTestRunner(verbosity=2)
    start_time = time.time()
    result = runner.run(suite)
    elapsed = time.time() - start_time

    log("\n-------------------------------------------------------")
    log("Exact Test Breakdown (Dynamically Derived from Test Runners):")
    log(f"  • Native C++ Assertion Checks: {cpp_checks_count} Passed (Exit code: {run_cpp_res.returncode})")
    log(f"  • Python test_protocol.py: {protocol_suite.countTestCases()} Tests")
    log(f"  • Python test_state_machine_logic.py: {state_machine_suite.countTestCases()} Tests")
    log(f"  • Python test_client_harness.py: {client_suite.countTestCases()} Tests (PTY Loopback & Disconnect)")
    log(f"  • Total Python Tests: {result.testsRun} Passed (Failures: {len(result.failures)}, Errors: {len(result.errors)})")
    log(f"Execution Time: {elapsed:.3f} seconds")
    overall_status = "PASS" if (result.wasSuccessful() and run_cpp_res.returncode == 0) else "FAIL"
    log(f"Overall Stage A.3 Final Verification Verdict: {overall_status}")
    log("-------------------------------------------------------\n")

    with open(log_file_path, "w") as f:
        f.write("\n".join(output_lines) + "\n")
    log(f"Saved complete execution report to: {log_file_path}")

    return 0 if overall_status == "PASS" else 1

if __name__ == '__main__':
    sys.exit(run_suite())
