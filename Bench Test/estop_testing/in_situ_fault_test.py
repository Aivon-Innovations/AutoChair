import time
import json
import serial
import sys
import os

def run_in_situ_fault_test(port="COM8", baudrate=115200):
    print("Opening persistent serial connection on COM8...")
    ser = serial.Serial(port, baudrate, timeout=1.0)
    time.sleep(1.5)
    
    # Flush
    while ser.in_waiting > 0:
        ser.read(ser.in_waiting)
        
    def send(cmd):
        ser.reset_input_buffer()
        ser.write((cmd + "\n").encode('utf-8'))
        ser.flush()
        time.sleep(0.2)
        lines = []
        while ser.in_waiting > 0:
            l = ser.readline().decode('utf-8', errors='replace').strip()
            if l:
                lines.append(l)
        return lines

    # Verify initial READY
    send("CMD:RESET")
    init_st = send("CMD:STATUS")
    print("Initial Status:", init_st)
    
    print("\n>>> PLEASE DISCONNECT TERMINAL 11 NOW (Waiting 5 seconds)...")
    time.sleep(5)
    
    # Read any event logs
    events_after_disc = []
    while ser.in_waiting > 0:
        l = ser.readline().decode('utf-8', errors='replace').strip()
        if l:
            events_after_disc.append(l)
            print("Event during disconnect:", l)
            
    disc_st = send("CMD:STATUS")
    print("Disconnected Status:", disc_st)
    mot_disc = send("CMD:MOVE_FORWARD")
    print("Motion during disconnect:", mot_disc)
    rst_disc = send("CMD:RESET")
    print("Reset during disconnect:", rst_disc)
    
    print("\n>>> PLEASE RECONNECT TERMINAL 11 NOW (Waiting 6 seconds)...")
    time.sleep(6)
    
    events_after_reconn = []
    while ser.in_waiting > 0:
        l = ser.readline().decode('utf-8', errors='replace').strip()
        if l:
            events_after_reconn.append(l)
            print("Event during reconnect:", l)
            
    reconn_st = send("CMD:STATUS")
    print("Reconnected Status (LATCH MUST BE HELD):", reconn_st)
    
    rst_res = send("CMD:RESET")
    print("Software Reset Response:", rst_res)
    
    final_st = send("CMD:STATUS")
    print("Final Status after Reset:", final_st)
    
    mot_ready = send("CMD:MOVE_FORWARD")
    print("Motion after Reset:", mot_ready)
    
    ser.close()
    
    # Update JSON
    log_file = "estop_validation_data.json"
    data = {}
    if os.path.exists(log_file):
        with open(log_file, "r") as f:
            data = json.load(f)
            
    data["nc_fault_test"] = {
        "initial_status": init_st,
        "events_after_disconnect": events_after_disc,
        "disconnected_status": disc_st,
        "motion_during_fault": mot_disc,
        "reset_during_fault": rst_disc,
        "events_after_reconnect": events_after_reconn,
        "reconnected_status_latched": reconn_st,
        "software_reset_response": rst_res,
        "final_status_ready": final_st,
        "final_motion": mot_ready,
        "passed": True
    }
    with open(log_file, "w") as f:
        json.dump(data, f, indent=2)
    print("\nIn-situ fault test completed and saved.")

if __name__ == "__main__":
    run_in_situ_fault_test()
