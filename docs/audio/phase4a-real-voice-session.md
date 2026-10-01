# Audio Phase 4A — Real Voice Session Runtime

## Status

**HARDWARE STATUS: PI_PENDING / DEVELOPMENT**

AutoChair is currently in **DEVELOPMENT / SIMULATED hardware mode**.
Voice commands are strictly decoupled from real wheelchair motors, GPIO motor control, ESP32 actuation, electronic braking, or any physical wheelchair movement.

Repository integration status:
- **Phase 2 (I2S capture):** HARDWARE INTEGRATED — verified on Raspberry Pi 4.
- **Phase 3 (Vosk STT foundation & utterance contract):** EXPERIMENTAL Pi validation (pre-repository integration) + unit tested in repository.
- **Phase 4A (Real Voice Session Runtime):** SOFTWARE IMPLEMENTED & UNIT TESTED — ready for deployment test on Raspberry Pi 4.

---

## Objective

Phase 4A introduces the `RealVoiceSession` coordinator ([voice_session.py](file:///Users/admin/Desktop/AutoChair/src/autochair/audio/voice_session.py)). This component bridges the physical streaming audio hardware pipeline (`AudioCapture` → `AudioPreprocessor` → `VoskSpeechRecognizer`) into a single, bounded, and testable voice session runtime.

The session owns the complete acquisition and recognition lifecycle:
1. Opening the capture device safely.
2. Streaming chunks through format conversion and speech recognition.
3. Returning immediately upon detecting a finalized utterance.
4. Finalizing buffered audio upon session timeout.
5. Guaranteed cleanup of ALSA hardware resources in all exit/error paths.

---

## Exact Data Flow

```
start session
    │
    ▼
capture.open()
    │
    ▼
┌──────────────────────────────────────────────────────────────┐
│ Streaming Capture Loop (max duration: default 5.0 s)          │
│                                                              │
│  capture.read_chunk()      Raw S32_LE stereo @ 48 kHz (ALSA) │
│       │                                                      │
│       ▼                                                      │
│  preprocessor.process()    Extract left ch, S32→S16,         │
│       │                    Hamming FIR resample 48→16 kHz    │
│       ▼                                                      │
│  recognizer.recognize()    Vosk streaming AcceptWaveform()   │
│       │                                                      │
│       ├─ Final utterance returned? ──► [Early Exit & Return] │
│       │                                                      │
│       └─ Incomplete/empty/unk? ─────► [Continue Loop]        │
└──────────────────────────────────────────────────────────────┘
    │
    ▼ (Loop duration expired)
recognizer.finalize()        Flush buffered audio from Vosk
    │
    ▼
capture.close()              (Guaranteed via finally block)
    │
    ▼
return final text (or "" on silence/unk)
```

---

## Session Policy: Fixed 5.0-Second Maximum Duration

Phase 4A uses a fixed, configurable session duration policy:
- **Default duration:** `5.0 seconds` (`AudioConfig.voice_session_duration_seconds = 5.0`).
- **Configurable:** Injectable per instance or configurable via `AudioConfig`.
- **Validation:** Durations $\le 0$ are rejected with `ValueError`.

### Why VAD (Voice Activity Detection) is not included yet:
1. **Pipeline Isolation:** Decouples hardware streaming lifecycle verification from silence detection threshold tuning.
2. **Deterministic Bounding:** A fixed maximum duration guarantees that the ALSA device cannot hang or loop indefinitely if ambient noise prevents silence detection.
3. **Pluggable Architecture:** The loop design in `RealVoiceSession` allows future VAD / energy-based end-of-utterance detectors to replace or supplement the fixed timeout without rewriting the underlying capture/preprocess/recognition pipeline.

---

## Experimentally Verified Pi Hardware vs Repository Status

The following components have been physically verified on Raspberry Pi 4 Model B (pre-repository hardware bench tests):
- **DFRobot MSM261S4030H0 I2S MEMS Microphone** (Left channel, LR tied to GND).
- **ALSA S32_LE Stereo 48 kHz Capture** via `snd-soc-simple-amplifier` / `googlevoicehat-soundcard` overlay.
- **AudioPreprocessor:** S32_LE stereo 48 kHz $\to$ S16_LE mono 16 kHz conversion with anti-aliasing FIR filter.
- **Vosk Small English Model (`vosk-model-small-en-us-0.15`)** with restricted grammar:
  - `start`
  - `stop`
  - `move forward`
  - `[unk]`
- **Vosk Final-Utterance Contract:** `recognize()` suppresses partial hypotheses and emits only completed utterances.

---

## What is NOT Integrated in Phase 4A

Phase 4A is strictly an isolated voice session runtime component. The following integrations are explicitly **NOT** performed in this phase:

- **`AudioManager` runtime loop:** Existing `MicrophoneManager` and `AudioManager.process()` remain untouched.
- **Touchscreen runtime integration:** No UI voice trigger or touch-to-listen binding.
- **Wheelchair motor control:** No PWM generation, motor drivers, or speed actuation.
- **ESP32 control:** No UART/I2C packets sent to microcontroller firmware.
- **Electronic braking:** No brake solenoid or safety relay actuation.
- **Autonomous navigation:** No obstacle avoidance or path planner integration.

---

## Safety Architecture

Voice recognition produces text utterances, which are converted to `InputCommand` instances via `VoiceCommandParser`. All commands must pass through `InputCommandValidator` and `SafetyManager` before any simulated motion can occur:

```
RealVoiceSession.start()
    │ (recognized text string)
    ▼
VoiceCommandParser.parse()
    │ (VoiceIntent / InputCommand)
    ▼
InputCommandValidator.validate()
    │ (Validation pass)
    ▼
CommandMapper.map()
    │ (Logical command mapping)
    ▼
SafetyManager.evaluate()
    │ (Obstacle, E-stop, and state validation)
    ▼
MotionCommand (SIMULATED ONLY — NO PHYSICAL MOTORS)
```
