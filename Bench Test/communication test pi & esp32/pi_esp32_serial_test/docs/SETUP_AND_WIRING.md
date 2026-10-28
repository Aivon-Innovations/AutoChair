# AutoChair — Standalone Raspberry Pi ↔ ESP32 Setup & Wiring Guide

**Document ID:** `DOC-WIRING-PI-ESP32-001`  
**Purpose:** Define the physical connection, electrical isolation, and SSH workflow for the standalone USB serial communication test.

---

## 1. Physical Architecture & Cable Connections

```text
       +-----------------------+
       |       MacBook         |
       |  (Terminal / SSH)     |
       +-----------+-----------+
                   |
                   | Local Wi-Fi / Ethernet Network (SSH Session)
                   v
       +-----------------------+
       |   Raspberry Pi 4B     |
       |  (AutoChair Host)     |
       |                       |
       | [USB-C Port] <========|=== 5V Power Supply (DO NOT rely on Mac data over USB-C)
       |                       |
       | [USB-A Port]          |
       +-----------+-----------+
                   |
                   | USB Data Cable (Type-A to Micro-USB / Type-C)
                   v
       +-----------------------+
       |    ESP32 Dev Module   |
       |  (Test Firmware)      |
       +-----------------------+
```

### Critical Hardware Notes
1. **Raspberry Pi 4 USB-C Port:**
   * The Pi 4's USB-C port is primarily designed for 5V DC power input. It does **not** provide a plug-and-play USB serial data interface to your Mac.
   * Reuse your established SSH workflow over Wi-Fi / Ethernet from your Mac terminal to operate the Pi.
2. **ESP32 Connection:**
   * Connect the ESP32 to one of the blue (USB 3.0) or black (USB 2.0) USB-A ports on the Raspberry Pi 4 using a known good USB **data** cable (not a charge-only cable).
   * Power to the ESP32 is supplied directly over the Pi's USB-A port (5V VBUS converted to 3.3V on the ESP32 dev board). No external power supply or battery is required.

---

## 2. Strict Electrical Isolation Checklist

> [!CAUTION]
> **SAFETY BOUNDARY:**
> This is a communication bench test only.
> * ❌ **DO NOT** connect the ESP32 to the wheelchair's 24V battery system.
> * ❌ **DO NOT** connect the ESP32 to the wheelchair motor controller, motors, or brakes.
> * ❌ **DO NOT** connect physical joystick or external high-voltage actuators.
> All movement commands in this test suite are purely simulated and logged via software.

---

## 3. Firmware Replacement Notice & Preservation

> [!WARNING]
> **FLASHING OVERWRITES EXISTING ESP32 FIRMWARE:**
> Flashing the standalone test firmware (`esp32_firmware`) will completely replace whatever firmware is currently resident in the ESP32's flash memory.
> 
> * If the connected ESP32 contains production or bench-validated firmware you wish to preserve, we will back up its flash image via `esptool.py read_flash` before writing new firmware, OR use a spare/secondary ESP32 dev board.
> * Flashing will **never** be performed without your explicit confirmation.
