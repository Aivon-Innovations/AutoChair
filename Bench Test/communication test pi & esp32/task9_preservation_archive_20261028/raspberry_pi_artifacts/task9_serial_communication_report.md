# AutoChair — Task 9: Raspberry Pi ↔ ESP32 Serial Communication Test

**Date:** 2026-10-28  
**Interface:** USB serial  
**Serial port:** /dev/ttyUSB0  
**Baud rate:** 115200  
**Test setup:** Raspberry Pi 4 ↔ ESP32 Dev Module  
**Wheelchair motors:** Disconnected

## Objective
Verify standalone serial communication, command handling, heartbeat monitoring,
safety-state transitions, and malformed-input handling.

## Test Results

| # | Test | Observed result | Verdict |
|---|---|---|---|
| 1 | Serial connection | Pi connected to ESP32 | PASS |
| 2 | PING | ACCEPTED; PONG | PASS |
| 3 | GET_STATUS | ACCEPTED; STATUS_OK | PASS |
| 4 | SET_MODE MANUAL | ACCEPTED; MODE_UPDATED | PASS |
| 5 | Motion command in IDLE | REJECTED; SYSTEM_NOT_ARMED_IDLE_STATE | PASS — expected rejection |
| 6 | RESET without healthy heartbeat | REJECTED; CANNOT_RESET_WITHOUT_HEALTHY_HEARTBEAT | PASS — expected rejection |
| 7 | RESET with heartbeat restored | ACCEPTED; SAFETY_LATCH_CLEARED_READY | PASS |
| 8 | STOP in READY | ACCEPTED; MOTION_STOPPED | PASS |
| 9 | Unknown command | REJECTED; UNKNOWN_COMMAND | PASS |
| 10 | Malformed JSON | MALFORMED_FRAME event received; state remained READY | PASS |
| 11 | Heartbeat timeout | HEARTBEAT_TIMEOUT event; state changed to latched SAFE | PASS |
| 12 | Status while SAFE | ACCEPTED; reported state SAFE | PASS |
| 13 | Recovery after heartbeat restored | ACCEPTED; state returned to READY | PASS |

## Additional Verification

- Native C++ tests: 92 assertion checks passed.
- Python tests: 37 passed, 0 failures, 0 errors.
- Host-side test report: logs/stage_a3_final_verification.log on the Mac.
- ESP32 telemetry identified ultrasonic, IMU, and encoder sensors as NOT_CONNECTED.
- Firmware reported simulated motion as STOP.

## Limitations

- Sensors were not connected during this bench test.
- Motion was simulated; no physical motor-control test was performed.
- This test does not establish real wheelchair safety or readiness for a person to use.
- The host-side unit tests and physical serial tests are distinct forms of evidence.

## Final Bench State

The final observed recovery result was READY, mode MANUAL, with heartbeat transmission enabled.

## Conclusion

The standalone Raspberry Pi-to-ESP32 USB serial test passed the listed protocol,
command-rejection, heartbeat-timeout, recovery, and malformed-frame checks under
the bench conditions described above.
