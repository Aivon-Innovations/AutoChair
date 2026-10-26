# AutoChair ESP32 — Hardware Wiring & Pin Mapping Specification

**Document Identification:** `AutoChair-alvira-dev/encoder today/03_HARDWARE_WIRING_AND_PIN_MAPPING.md`  
**Date:** 2026-10-23  
**Project:** AutoChair — AI-Powered Smart Wheelchair  
**Subsystem:** Electrical Pin Budget, Interface Schematics & Wiring  
**Status:** VERIFIED WIRING & PIN ALLOCATION BASELINE  

---

## 1. Hardware Specifications: Claimed vs. Verified

The wheel encoder hardware utilized in this project is documented under the following specifications:

| Parameter | Manufacturer Claim / Specification | Verification Status | Source Reference |
| :--- | :--- | :---: | :--- |
| **Model Number** | Pro:Range 3806-OPTI-600-ABZ-OC | **REPORTED** | Enclosure / Vendor Label |
| **Sensor Architecture** | Incremental Optical Rotary Encoder | **CONFIRMED** | Physical Bench Inspection |
| **Channels** | Channel A, Channel B (Quadrature), Channel Z (Index) | **CONFIRMED** | Electrical Bench Observation |
| **Nominal Resolution** | $600\text{ pulses/revolution (P/R)}$ ($1,200$ edges/ch) | **REPORTED (PPR)** | Manufacturer Specification |
| **Decoded Resolution** | $2,400\text{ counts/revolution (X4 CPR)}$ | **UNVERIFIED (Right)** | $2,432.7$ avg measured on Left; Right unmeasured |
| **Output Stage Type** | NPN Open-Collector (Open-Drain sinking transistor) | **CONFIRMED** | Requires pull-ups; floats without rail |
| **Supply Voltage (VCC)** | $5.0\text{ V DC}$ to $24.0\text{ V DC}$ | **MEASURED** | $5.0066\text{ V DC}$ measured on bench rail |
| **Signal Logic Level** | Pull-up dependent (interfaced to $3.3\text{ V}$ for ESP32) | **MEASURED** | $3.307\text{ V DC}$ measured on pull-up rail |
| **Harness Lead Colors** | Red=VCC, Black=GND, Green=A, White=B, Yellow=Z | **CONFIRMED** | Standard Harness Color Code |
| **Outer Cable Shield** | Braided drain wire | **CONFIRMED** | Electrically isolated / left unconnected |

> [!IMPORTANT]
> **Distinction Between Claims and Empirical Facts:**  
> The nominal $600\text{ PPR}$ ($2,400\text{ CPR}$) specification is a manufacturer product claim. While the Left encoder exhibited an empirical average of $2,432.7\text{ counts/rev}$ across optical pulses in earlier tests, the Right encoder currently on the bench has **NOT** undergone precision calibration or multi-revolution optical ratio verification.

---

## 2. Microcontroller Pin Budget & System Pin Allocation

The ESP32-WROOM-32D 38-pin DevKit incorporates 34 physical GPIO lines. Below is the master pin assignment table reconciling all subsystem connections:

| Pin Function | GPIO | Pin Type | Subsystem Assignment | Safety / Conflict Status |
| :--- | :---: | :---: | :--- | :--- |
| **Left Encoder A** | **GPIO25** | Input | Left Wheel Channel A (Green) | Bench Verified (4.7 kΩ pull-up to 3.3V) |
| **Left Encoder B** | **GPIO26** | Input | Left Wheel Channel B (White) | Bench Verified (4.7 kΩ pull-up to 3.3V) |
| **Left Encoder Z** | **GPIO27** | Input | Left Wheel Channel Z (Yellow) | Bench Verified (4.7 kΩ pull-up to 3.3V) |
| **Right Encoder A** | **GPIO32** | Input | Right Wheel Channel A (Green) | **VERIFIED SAFE ALTERNATIVE** |
| **Right Encoder B** | **GPIO17** | Input | Right Wheel Channel B (White) | Verified General I/O (4.7 kΩ pull-up) |
| **Right Encoder Z** | **GPIO14** | Input | Right Wheel Channel Z (Yellow) | Verified General I/O (4.7 kΩ pull-up) |
| Ultrasonic Triggers | GPIO 4, 5, 18, 19, 23, 13 | Output | TRIG 0 to 5 | Dedicated |
| Ultrasonic Echoes | GPIO 34, 35, 36, 39, 0, 33 | Input | ECHO 0 to 5 | Dedicated (ECHO 4 placeholder 0) |
| IMU (MPU6050) | GPIO 21 / 22 | I2C | SDA / SCL | Dedicated open-drain bus |
| Programming UART | GPIO 1 / 3 | UART | TXD0 / RXD0 | Dedicated to CP2102 USB Bridge |
| System LED / Boot | GPIO 2 / 0 | System | Onboard LED / Bootloader strapping | Internal / Pull-down guarded |

---

## 3. Critical Hardware Conflict Analysis: GPIO16 Rejection vs. GPIO32

### 3.1 Root Cause of the GPIO16 Boot Failure (`flash read err, 988`)
In early design iterations, Right Encoder Channel A was mapped to `GPIO16`. This mapping was rejected based on the following hardware realities:
1. **Internal Flash Bus Aliasing:** On the ESP32-WROOM-32D module, `GPIO16` and `GPIO17` are internally connected to the secondary SPI bus (used for PSRAM or flash chip select on modules with embedded RAM).
2. **Open-Collector Grounding Risk:** Optical encoders feature NPN open-collector output transistors. When an optical disc slit aligns with the internal phototransistor, the output transistor conducts, pulling the signal line directly to **GND (~0.1V - 0.2V)**.
3. **50% Probability Boot Lockout:** Because a 600 PPR encoder disc has a ~50% duty cycle, there is an approximate 50% statistical probability that whenever the wheelchair comes to rest, Channel A's transistor is ON.
4. **Permanent Failure Mechanism:** If the ESP32 reboots, resets, or powers up while the wheel is resting in an active slit position, `GPIO16` is held hard at GND. This disrupts the SPI flash bus, causing the internal ROM bootloader to enter a hard crash loop:
   ```text
   rst:0x10 (RTCWDT_RTC_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
   flash read err, 988
   ets_main.c 371
   ```
   Under this condition, `esptool.py flash_id` returns `0x00 0x000000` or `0xff 0xffffff` (total loss of flash communication). External pull-up resistors cannot override a conducting NPN transistor.

### 3.2 Verification of GPIO32 as the Safe Alternative
`GPIO32` was chosen and verified based on the following architectural properties:
* **Silicon Independence:** `GPIO32` belongs to the ADC1 block. It has **zero** strapping functions, zero bootloader modes, and zero physical connection to the SPI flash controller.
* **Boot Stability:** Holding `GPIO32` LOW, HIGH, or floating during boot has zero impact on microcontroller startup.
* **Interrupt Capability:** Supports edge-triggered hardware interrupts (`attachInterrupt(..., CHANGE)`).
* **Physical Location:** Header Pin 7 on the left header row of the DevKit, directly adjacent to GPIO14 and the Left encoder cluster.

---

## 4. Physical Wiring Schematic & Intended Connections

### 4.1 Right Encoder Wiring Schematic
The Right Wheel Encoder is connected to the ESP32 and breadboard distribution rails as follows:

```text
       +5V Rail (ESP32 5V Pin) ────────────────────────── Right Encoder VCC (Red)
       GND Rail (ESP32 GND Pin) ───────────────────────── Right Encoder GND (Black)
                                                
       +3.3V Rail (ESP32 3V3 Pin)
            │
            ├────[ 4.7 kΩ ]────┬──────── Right Encoder Channel A (Green)  ──► ESP32 GPIO 32
            │                  │
            ├────[ 4.7 kΩ ]────┼──────── Right Encoder Channel B (White)  ──► ESP32 GPIO 17
            │                  │
            └────[ 4.7 kΩ ]────┴──────── Right Encoder Channel Z (Yellow) ──► ESP32 GPIO 14

       Outer Cable Shield / Braid ─────────────────────── Left UNCONNECTED & Insulated
```

### 4.2 Terminal & Pin Summary

| Signal Name | Harness Wire Color | DevKit Pin Header | ESP32 GPIO | External Conditioning |
| :--- | :--- | :--- | :---: | :--- |
| **Power Supply** | Red | Pin 19 (Right) | **5V** | Direct from 5V USB bus |
| **Signal Ground** | Black | Pin 14 (Left) | **GND** | Common ground with ESP32 |
| **Channel A** | Green | Pin 7 (Left) | **GPIO32** | $4.7\text{ k}\Omega$ pull-up to 3.3V |
| **Channel B** | White | Pin 9 (Right) | **GPIO17** | $4.7\text{ k}\Omega$ pull-up to 3.3V |
| **Channel Z (Index)** | Yellow | Pin 13 (Left) | **GPIO14** | $4.7\text{ k}\Omega$ pull-up to 3.3V |
| **Shield** | Bare Copper / Braid | — | **NC** | Unconnected, insulated with heat shrink |

---

## 5. Breadboard 3.3V Distribution Rail Fault & Rectification

### 5.1 Fault Diagnosis
During early bench testing, the Right encoder exhibited catastrophic noise:
* The three $4.7\text{ k}\Omega$ pull-up resistors were correctly plugged into the breadboard tie-points for Green, White, and Yellow signals.
* However, the common supply row for the pull-up resistors was **not jumpered to the ESP32 3V3 pin**.
* Open-collector outputs with unpowered pull-ups behave as high-impedance floating nodes when the internal phototransistor is dark.
* High-impedance CMOS inputs on the ESP32 picked up ambient 50 Hz hum and capacitive noise, generating hundreds of spurious transitions per second.

### 5.2 Physical Correction
* The operator placed a solid jumper wire between the ESP32 3V3 header pin and the breadboard pull-up distribution bus.
* This established a verified $3.307\text{ V DC}$ reference across all three pull-up resistors.
* **Verification Note:** The operator physically inspected and confirmed the breadboard connections and verified that the wiring was restored to original intended configuration after diagnostic swap tests. The engineering agent verified the electrical correction via serial telemetry baselines.

---

## 6. Physical Isolation & Safety Boundaries

* **Left Encoder State:** The Left encoder signal wires (GPIO25, 26, 27) were physically disconnected from the breadboard during Right encoder testing to isolate channel crosstalk.
* **Power Stage State:** Wheelchair motors, motor controllers, 24V chassis battery harnesses, and power relays remained **completely disconnected**.
* **Multimeter Verification:** Voltage levels ($5.0066\text{ V}$ and $3.307\text{ V}$) were established using bench DMM measurements documented in project records.
