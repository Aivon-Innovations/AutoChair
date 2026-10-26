# AutoChair ESP32 — Executive Engineering Handover

**Document Identification:** `AutoChair-alvira-dev/encoder today/06_EXECUTIVE_HANDOVER.md`  
**Date:** 2026-10-23  
**Project:** AutoChair — AI-Powered Smart Wheelchair  
**Subsystem:** Dual Optical Wheel Encoder Subsystem  
**Status:** COMPLETE EXECUTIVE HANDOVER  

---

## Executive Summary: Nine Core Engineering Answers

### 1. What did we achieve today?
We resolved critical hardware conflicts on the ESP32-WROOM-32D, physically established safe pin mappings for the **Right Wheel Encoder** (remapping Channel A to **`GPIO32`** and rejecting flash-conflicting `GPIO16`), diagnosed and cured floating open-collector signal noise by connecting the breadboard 3.3V pull-up distribution bus, validated active quadrature pulse accumulation on Channels A & B ($>187,000$ counts with zero invalid transitions), and proved electrical continuity and hardware interrupt triggering on Channel Z (`GPIO14`).

### 2. What problem did the 3.3V rail correction solve?
The encoder outputs are NPN open-collector transistors that require external pull-up resistors to establish CMOS HIGH logic levels. Initially, the three $4.7\text{ k}\Omega$ pull-up resistors on the breadboard were not connected to the ESP32 3.3V pin. This caused the signal lines to float near GND, inducing electrical floating noise on high-impedance inputs that caused false index and count increments while stationary. Connecting the 3.3V distribution bus pulled the lines firmly to $3.3\text{ V DC}$ and created a completely rock-solid stationary baseline ($\Delta = 0$ drift).

### 3. What did the Right Encoder demonstrate successfully?
* **Quadrature Decoding:** Channels A (GPIO32) and B (GPIO17) actively accumulated $>187,000$ counts with strictly zero invalid transitions added (`inv=1` maintained).
* **Speed Tracking:** Dynamic RPM telemetry accurately reflected hand rotation velocity up to $>200\text{ RPM}$ and returned cleanly to `0.00 RPM` when stopped.
* **Interrupt Vector Continuity:** Channel Z (GPIO14) hardware interrupt `isrZ1` actively triggers on rising edges and increments `_indexCount` in firmware.
* **Stationary Stability:** Across repeated stationary observation windows (22.5s, 5.76s, 6.4s), count and index drift were strictly zero.

### 4. Why is Z-index calibration still unresolved?
* **Telemetry Sampling Granularity:** Serial telemetry logs at 2 Hz (every 500 ms). Optical index pulses at speed last $< 2\text{ ms}$, so discrete polling snapshots (`Z=0` or `1`) cannot measure pulse width or waveform cleanliness.
* **Marked Revolution Jitter:** Controlled manual rotation trials from visual reference marks produced $+2$ index increments. Because the shaft was turned by hand without mechanical end-stops, manual visual overshoot, boundary oscillation across the optical slit, and multi-slit disc architecture remain undistinguished.
* **Buffer Flush in Capture Script:** The diagnostic script `capture_until_stop.py` executed `reset_input_buffer()` on startup, discarding intermediate transition frames and capturing only post-rotation stationary data.

### 5. What did the old PulseView investigation prove, and which encoder did it test?
* **Scope of Prior Test:** The historical AA161 / PulseView test was conducted on a **macOS workstation exclusively on the LEFT Wheel Encoder** (GPIO 25, 26, 27).
* **What it Proved:** It proved clean quadrature square waves, A-leads-B (CW) and B-leads-A (CCW) phase relationships, and an average count of $2,432.7\text{ counts/rev}$ across optical pulses on the Left encoder.
* **Evidentiary Limitation:** The Right encoder was **never tested on a logic analyzer**. Furthermore, an exhaustive search across the host workstation revealed that no raw `.sr` or `.sal` session files were preserved in the repository; only summarized markdown reports exist.

### 6. What is the current firmware/software status?
* **Host Unit Tests:** **119 / 119 native tests PASSED (100%)** covering HAL, drivers, odometry kinematics, health monitoring, and system fault-injection.
* **Target Build:** Clean ESP32 build (`pio run -e esp32dev`) with 0 errors and 0 warnings (RAM: 7.2%, Flash: 23.6%).
* **Runtime Verification:** ESP32 boots cleanly into `State=3 (IDLE)`, `Safety=0 (Clear)`, `Mode=0 (Simulation)` with real-time telemetry streaming on `COM8` at 115200 baud.

### 7. What are the remaining technical risks?
* **Odometry Scaling Error:** The physical mounting location (motor shaft vs. gearbox vs. wheel hub) is unverified. If mounted on the motor side of the $23.3:1$ gearbox, resolution is $\approx 55,920\text{ counts/wheel rev}$; if on the wheel hub, it is $\approx 2,400\text{ counts/wheel rev}$. Attempting closed-loop speed control before physical calibration will cause runaway drive conditions.
* **Boundary Chattering:** The 20 ms index debounce filter does not reject boundary rocking if the shaft oscillates near an index slit for $> 20\text{ ms}$.
* **EMI Under Motor Load:** Signal noise immunity has only been verified on an isolated bench with motors disconnected.

### 8. What must NOT be claimed as verified?
1. Do **NOT** claim that the Right encoder has exactly 1 index pulse per revolution.
2. Do **NOT** claim that the Right encoder has a verified CPR of 2,400 or PPR of 600.
3. Do **NOT** claim that Left encoder measurements apply identically to the Right encoder.
4. Do **NOT** claim that the Right encoder reverse (CCW) direction polarity has been verified.
5. Do **NOT** claim that passing 119 native unit tests proves physical wheelchair safety.

### 9. What is the recommended next engineering decision?
Given that the bench operator has decided not to perform further PulseView or logic analyzer testing:
1. **Freeze Bench Testing:** Respect the operator's decision and conclude manual bench diagnostics.
2. **Harmonize Documentation:** Complete repository documentation refactoring, annotating all nominal calibration parameters ($600\text{ PPR}$, $2,400\text{ CPR}$, $23.3:1$ ratio) as uncalibrated placeholders.
3. **Firmware Quadrature Gating:** Enhance `handleIndex()` in firmware to require a minimum quadrature count advance ($\approx 2,000+$ counts) between accepted index pulses to eliminate boundary oscillation chatter.
4. **Defer Physical Calibration to Mechanical Lab:** Schedule formal waveform characterization and chassis roll calibration for the future mechanical integration phase when motorized test fixtures and digital oscilloscopes are available.
