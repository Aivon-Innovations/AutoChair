# Audio Phase 2 — Real I2S Microphone Integration

## Status

**INTEGRATED — AUDIO CAPTURE VERIFIED**

---

## Objective

The objective of Phase 2 was to connect the real DFRobot MSM261S4030H0R I2S MEMS
microphone to the Raspberry Pi 4 and verify that actual audio capture works correctly
before any speech-recognition software is integrated.

This phase proves the hardware path from the physical microphone through the I2S
interface to ALSA PCM capture. It does **not** implement speech recognition, voice
command processing, or any connection to the wheelchair control system.

---

## Hardware

| Component | Detail |
|---|---|
| Raspberry Pi | Raspberry Pi 4 Model B Rev 1.5 |
| Microphone IC | MSM261S4030H0R |
| Module | DFRobot Fermion I2S MEMS Microphone (SEN0526) |
| Interface | Digital I2S |
| Supply voltage | 3.3V |

---

## Wiring

All connections verified on the physical Raspberry Pi 4 Model B Rev 1.5.

| Microphone Pin | Raspberry Pi Connection | Physical Pin | Notes |
|---|---|---|---|
| VDD | 3.3V | Pin 1 | |
| GND | GND | Pin 6 | |
| SCK | GPIO18 / PCM_CLK | Pin 12 | I2S bit clock |
| WS | GPIO19 / PCM_FS | Pin 35 | I2S frame sync / word select |
| SD | GPIO20 / PCM_DIN | Pin 38 | I2S data from microphone to Pi |
| L/R | GND | Pin 39 | LOW = left channel selected |

**L/R pin is connected to GND (LOW).** This selects the left I2S channel. The right
channel carries no audio data from this microphone (right channel samples are zero),
which is the expected and correct behaviour for a single-microphone left-channel
configuration.

---

## I2S Protocol (MSM261S4030H0R — Confirmed from Datasheet)

| Parameter | Value |
|---|---|
| Device role | I2S slave (microphone) |
| Audio data width | 24-bit |
| Word width | 32-bit (24-bit data in 32-bit word) |
| TDM slots | 2 (stereo I2S frame) |
| Slot width | 32 bits |
| SCK cycles per frame | 64 (2 slots × 32 bits) |
| Nominal SCK | 3.072 MHz (48 kHz × 64) |
| WS frequency | 48 kHz |
| Pi role | Bit-clock master + frame-clock master |

---

## Pi I2S Configuration

I2S was enabled in `/boot/firmware/config.txt` (Raspberry Pi OS Bookworm path):

```
dtparam=i2s=on
dtoverlay=msm261s4030h0
```

A configuration backup was created before any changes were applied:

```
/boot/firmware/config.txt.before-msm261
```

---

## Device Tree Overlay

A custom Device Tree overlay was written, compiled, and installed specifically for this
microphone. The overlay source is version-controlled at:

```
hardware/audio/msm261s4030h0.dts
```

The overlay uses:

- **`brcm,bcm2835`** I2S controller (`i2s_clk_producer`)
- **`simple-audio-card`** driver
- **`dmic-codec`** as the microphone codec representation
- Pi I2S controller as **bit-clock master**
- Pi I2S controller as **frame-clock master**
- **2 TDM slots**, **32-bit slot width**

Compiled and installed on the Pi as:

```
/boot/firmware/overlays/msm261s4030h0.dtbo
```

**Note:** `googlevoicehat-soundcard` was evaluated and not used. It is designed for a
different product (AIY Voice HAT) and carries an unacceptable risk of GPIO 16 conflicts
with other AutoChair hardware.

---

## ALSA Verification

After reboot, the ALSA subsystem detected the microphone as a capture device:

```
card 3: MSM261S4030H0 [MSM261S4030H0],
device 0: bcm2835-i2s-dmic-hifi dmic-hifi-0
```

ALSA hardware parameters were successfully queried.

**Supported formats:**

| Format | Description |
|---|---|
| S16_LE | 16-bit signed little-endian |
| S24_LE | 24-bit signed little-endian |
| S32_LE | 32-bit signed little-endian |

**Channels:** 2 (stereo frame; left channel carries microphone audio, right is silent)

**48 kHz:** Supported and verified.

---

## Capture Test

The following `arecord` command was executed successfully on the Raspberry Pi:

```bash
arecord -D hw:3,0 -c 2 -r 48000 -f S32_LE -t wav -d 5 ~/autochair/mic_test.wav
```

| Parameter | Value | Reason |
|---|---|---|
| `-D hw:3,0` | Card 3, device 0 | ALSA card assigned by kernel after overlay load |
| `-c 2` | 2 channels (stereo) | I2S frame is inherently stereo |
| `-r 48000` | 48 kHz sample rate | Nominal rate per MSM261S4030H0 datasheet |
| `-f S32_LE` | 32-bit little-endian | Pi I2S peripheral pads 24-bit data to 32-bit words |
| `-t wav` | WAV container | Standard uncompressed audio format |
| `-d 5` | 5-second duration | Capture window for test |

---

## WAV Verification

The resulting WAV file was verified to have the following properties:

| Property | Value |
|---|---|
| Format | 32-bit PCM (S32_LE) |
| Sample rate | 48000 Hz |
| Channels | 2 (stereo) |
| Duration | 5 seconds |
| File size | ~1.9 MB |

---

## Sample Verification

Per-channel sample statistics from the 5-second recording:

**Left channel** (microphone audio):

| Statistic | Value |
|---|---|
| Minimum | −261,405,696 |
| Maximum | +224,859,584 |
| Mean | 205,159.15 |
| Std deviation | 26,775,371.15 |

**Right channel** (expected silent):

| Statistic | Value |
|---|---|
| Minimum | 0 |
| Maximum | 0 |
| Mean | 0 |
| Std deviation | 0 |

**Interpretation:**

- The left channel contains varying non-zero samples, confirming real audio signal
  capture from the MSM261S4030H0R microphone.
- The right channel is identically zero across all samples. This is the expected and
  correct result: with L/R tied LOW (GND), the microphone outputs audio only on the
  left channel. The right channel of the stereo I2S frame carries no microphone data.
- These statistics are consistent with the MSM261S4030H0R datasheet specification for
  single-microphone left-channel operation.

---

## Audible Verification

The `mic_test.wav` file was transferred from the Raspberry Pi to the Mac development
workstation and played back via macOS audio output.

> A 5-second recording containing repeated spoken "AutoChair" was transferred to the
> Mac and played back successfully. The recorded speech was audibly heard.

This confirms the complete physical audio path:
**Voice → MSM261S4030H0R microphone → I2S → Raspberry Pi 4 → ALSA → WAV file**

The `mic_test.wav` file is **not committed to the repository** (it is a personal voice
recording and a binary test artefact).

---

## Current Result

> **REAL I2S MICROPHONE CAPTURE VERIFIED.**

The DFRobot MSM261S4030H0R I2S MEMS microphone is physically connected, kernel-
configured, ALSA-detected, and producing audible voice recordings on Raspberry Pi 4.

---

## Not Yet Completed

The following components remain to be implemented in future phases:

- Speech recognition engine (Vosk or Whisper — not yet selected or installed)
- Speech-to-text integration with the AutoChair audio pipeline
- Real voice command recognition
- Text-to-speech (TTS) audio output
- PAM8403 amplifier and speaker output
- Buzzer integration (GPIO alert hardware)
- Touchscreen voice-status display integration
- Voice-to-safety/control integration
- Real wheelchair motor control through voice

---

## Safety and Architecture Reminder

Voice input in AutoChair must always flow through the validated pipeline:

```
VOICE → COMMAND PARSER → INPUT VALIDATION → SAFETY LAYER → MOTION CONTROL
```

**This Phase 2 test did not connect voice audio directly to the wheelchair motors.**
The microphone capture test is isolated at the ALSA layer. No speech was recognised,
no `InputCommand` was generated, and no motion was triggered.

The existing AutoChair input/safety/motion architecture remains untouched.

---

## Related Files

| File | Description |
|---|---|
| [`hardware/audio/msm261s4030h0.dts`](../../hardware/audio/msm261s4030h0.dts) | Verified Device Tree overlay source |
| [`src/autochair/audio/devices/microphone.py`](../../src/autochair/audio/devices/microphone.py) | `MicrophoneManager` ABC (Phase 1) — real I2S driver to replace `SimulatedMicrophoneManager` in Phase 3 |
| [`src/autochair/audio/state.py`](../../src/autochair/audio/state.py) | `AudioState` enum (Phase 1) |
| [`src/autochair/audio/manager.py`](../../src/autochair/audio/manager.py) | `AudioManager` (Phase 1) |
