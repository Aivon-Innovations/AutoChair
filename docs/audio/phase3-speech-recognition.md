# Audio Phase 3 — Speech Recognition Architecture

## Status

**SOFTWARE IMPLEMENTED — EXPERIMENTAL PI VALIDATION ONLY**

Phase 3 software is complete and tested on Mac (297 tests passing).

Hardware integration status:
- **Phase 2 (I2S capture):** HARDWARE INTEGRATED — verified on Raspberry Pi 4
- **Phase 3 (Vosk STT):** EXPERIMENTAL Pi validation only (pre-repository integration)
- **Full repository deployment and test on Pi:** NOT YET PERFORMED

---

## Objective

Phase 3 introduces the speech-recognition layer that converts real microphone
audio into recognized text and routes it through the existing AutoChair
command validation and safety pipeline.

This phase does NOT implement:
- Direct motor control
- Voice-to-motion without safety validation
- Wake word detection
- Cloud speech services
- TTS output hardware
- PAM8403 amplifier or speaker output
- Buzzer integration

---

## Architecture

```
AudioCapture.read_chunk()          ← raw S32_LE stereo at 48 kHz
        │
        ▼
AudioPreprocessor.process()        ← extract left channel, S32→S16, resample 48→16 kHz
        │
        ▼
VoskSpeechRecognizer.recognize()   ← bytes → recognized text (grammar-restricted)
        │
        ▼
VoiceCommandParser.parse()         ← text → ParseResult / InputCommand
        │
        ▼
InputCommandValidator.validate()   ← existing (unchanged)
        │
        ▼
CommandMapper.map()                ← existing (unchanged)
        │
        ▼
SafetyManager.evaluate()           ← existing (unchanged)
        │
        ▼
MotionCommand (SIMULATED — logging only, no hardware movement)
```

**Voice never bypasses `InputCommandValidator` or `SafetyManager`.**
**Voice never produces a `MotionCommand` directly.**

---

## New Files Added

| File | Purpose |
|---|---|
| [`src/autochair/audio/config.py`](../../src/autochair/audio/config.py) | `AudioConfig` dataclass — central configuration |
| [`src/autochair/audio/devices/capture.py`](../../src/autochair/audio/devices/capture.py) | `AudioCapture` ABC + `SimulatedAudioCapture` + `ALSAAudioCapture` |
| [`src/autochair/audio/devices/preprocessor.py`](../../src/autochair/audio/devices/preprocessor.py) | `AudioPreprocessor` — channel extract, S32→S16, resample |
| [`src/autochair/audio/speech/vosk_recognizer.py`](../../src/autochair/audio/speech/vosk_recognizer.py) | `VoskSpeechRecognizer` — Vosk KaldiRecognizer with grammar |
| [`tests/audio/conftest.py`](../../tests/audio/conftest.py) | Session-scoped vosk mock for Mac tests |
| [`tests/audio/test_audio_config.py`](../../tests/audio/test_audio_config.py) | AudioConfig tests |
| [`tests/audio/test_audio_capture.py`](../../tests/audio/test_audio_capture.py) | AudioCapture tests |
| [`tests/audio/test_audio_preprocessor.py`](../../tests/audio/test_audio_preprocessor.py) | AudioPreprocessor tests |
| [`tests/audio/test_vosk_recognizer.py`](../../tests/audio/test_vosk_recognizer.py) | VoskSpeechRecognizer + pipeline tests |

---

## AudioConfig

`AudioConfig` is a central `@dataclass` that holds all hardware-specific and
model-specific configuration values.  Nothing is hard-coded inside the
implementation classes; all values flow from `AudioConfig`.

Key fields:

| Field | Default | Description |
|---|---|---|
| `alsa_device` | `hw:CARD=MSM261S4030H0,DEV=0` | Stable ALSA device name (not numeric card index) |
| `capture_sample_rate` | `48000` | Verified Pi hardware rate |
| `capture_channels` | `2` | Stereo I2S frame |
| `capture_format_bits` | `32` | S32_LE — Pi I2S pads 24-bit data to 32-bit words |
| `mic_channel` | `0` | Left channel (LR tied LOW) |
| `vosk_model_path` | `""` | Path to Vosk model directory on device |
| `vosk_sample_rate` | `16000` | Vosk model input rate |
| `vosk_grammar` | `["start","stop","move forward","[unk]"]` | Restricted command set |
| `vosk_suppress_logs` | `True` | Suppress Vosk C++ verbose output |

---

## AudioCapture

The `AudioCapture` ABC provides a device-independent interface for reading
raw PCM chunks.

```
AudioCapture
  ├── SimulatedAudioCapture   (Mac / tests / development — no hardware)
  └── ALSAAudioCapture        (Raspberry Pi — requires pyalsaaudio)
```

`ALSAAudioCapture` uses a **lazy import** of `pyalsaaudio`.  Importing
`capture.py` on Mac does **not** fail.  Only instantiating `ALSAAudioCapture`
triggers the import and will raise `ImportError` if pyalsaaudio is absent.

`ALSAAudioCapture` uses `config.alsa_device` (e.g.
`"hw:CARD=MSM261S4030H0,DEV=0"`) rather than a numeric card index to
avoid ALSA card re-numbering when other audio devices are added.

---

## AudioPreprocessor

Converts raw ALSA bytes to Vosk-ready PCM in three steps:

### Step 1: Channel extraction
Unpacks interleaved S32_LE stereo bytes and extracts `config.mic_channel`
(default: left channel, index 0).

### Step 2: S32 → S16 conversion
The MSM261S4030H0R outputs 24-bit audio inside 32-bit I2S words (left-
justified in the slot; lower 8 bits are padding zeros).

```
s16 = clamp(s32 >> 16, -32768, 32767)
```

Right-shifting by 16 extracts the top 16 bits of the 24-bit audio data.
The result is clamped to the S16 range to guard against edge cases.

### Step 3: Resampling 48 kHz → 16 kHz

The temporary Pi experiment used simple decimation (every 3rd sample)
for rapid diagnosis.  Phase 3 uses a **Hamming-windowed sinc FIR filter**
(15 taps, cutoff ≈ 8 kHz) before decimation by factor 3.

The FIR filter:
- Attenuates frequencies above 8 kHz before decimation (anti-aliasing)
- Provides ~41 dB stop-band attenuation (adequate for 16-bit speech)
- Preserves speech frequencies (300 Hz – 3.4 kHz) with minimal distortion
- Is pre-computed once at module load (zero per-call overhead)
- Uses stdlib only (`math`, `struct`, `array`) — no NumPy, no SciPy

To replace the resampler with a higher-quality implementation (e.g. SoxR):
subclass `AudioPreprocessor` and override `resample()`.

---

## VoskSpeechRecognizer

Implements the existing `SpeechRecognizer` ABC using Vosk's
`KaldiRecognizer` with a restricted grammar.

### Grammar (restricted — Phase 3)

```python
["start", "stop", "move forward", "[unk]"]
```

Only these phrases (and `"[unk]"` for unknown speech) will ever be
returned by Vosk.  This is enforced at the `KaldiRecognizer` level —
any other utterance maps to `"[unk]"`.

`"[unk]"` is converted to `""` (empty string) by `_extract_text()` so
that `VoiceCommandParser` receives empty input and produces
`VoiceIntent.UNKNOWN`, generating no motion command.

### Lazy import

`vosk` is imported inside `__init__()`.  Importing `vosk_recognizer.py`
on Mac without vosk installed does **not** fail.  Only instantiation
raises `ImportError`.

In tests, `sys.modules["vosk"]` is replaced by a `MagicMock` in
`conftest.py` (session-scoped, automatic) so all Vosk tests run on Mac
without the library installed.

### Model loading

Vosk model loading may take 0.5–3 s on the Raspberry Pi 4.  Call
`VoskSpeechRecognizer.__init__()` at application startup, not on the
first audio frame.

### Streaming / stateful

The `KaldiRecognizer` instance is kept alive and reused across calls.
`recognize(bytes)` feeds one audio chunk and returns any completed
utterance.  Call `finalize()` at end-of-stream to flush the buffer.
Call `reset()` to clear state between distinct utterances.

---

## Initial Supported Voice Commands (Phase 3)

| Spoken phrase | VoiceIntent | InputCommand | Notes |
|---|---|---|---|
| "stop" | STOP | `InputCommand("STOP", VOICE)` | Passes validator + safety |
| "move forward" | MOVE_FORWARD | `InputCommand("FORWARD", VOICE)` | Passes validator + safety |
| "start" | START | `None` | System intent — no motion (design gap) |
| Any other / silence | UNKNOWN | `None` | No command generated |

**"AutoChair" is NOT a wake word in this phase.** It was not recognized by
the restricted grammar during the Pi experiment.

Future commands (planned, not yet implemented):
- move backward, turn left, turn right, slow down, speed up

---

## Simulation vs Real Hardware

| Component | Development (Mac) | Raspberry Pi |
|---|---|---|
| Audio capture | `SimulatedAudioCapture` | `ALSAAudioCapture` |
| Preprocessing | `AudioPreprocessor` (same code) | `AudioPreprocessor` (same code) |
| Speech recognition | `SimulatedSpeechRecognizer` | `VoskSpeechRecognizer` |
| Voice command parser | `VoiceCommandParser` (same code) | `VoiceCommandParser` (same code) |
| Validation / safety | Existing pipeline (same code) | Existing pipeline (same code) |

The default development mode (`HARDWARE_STATUS = "SIMULATED"`) does **not**
require the physical microphone or the Vosk model.

---

## Dependencies

### Core project (no change)
No new **required** dependencies.  The project runs on Mac without
pyalsaaudio or vosk.

### Pi-only optional extras (`pi-audio`)

Added to `pyproject.toml`:

```toml
[project.optional-dependencies]
pi-audio = [
    "pyalsaaudio>=0.11.0",
    "vosk>=0.3.45",
]
```

Install on Raspberry Pi:
```bash
pip install -e ".[pi-audio]"
```

Download Vosk model separately:
```bash
wget https://alphacephei.com/vosk/models/vosk-model-small-en-us-0.15.zip
unzip vosk-model-small-en-us-0.15.zip
```

---

## Raspberry Pi Deployment Steps (not yet performed)

The following steps are required before claiming full Phase 3 validation:

```bash
# 1. Ensure Phase 2 microphone is verified (arecord -l shows MSM261S4030H0)
arecord -l

# 2. Install Pi-audio extras
cd ~/autochair
pip install -e ".[pi-audio]"

# 3. Download Vosk model (if not already present)
wget https://alphacephei.com/vosk/models/vosk-model-small-en-us-0.15.zip
unzip vosk-model-small-en-us-0.15.zip

# 4. Pull alvira-dev branch
git fetch origin
git checkout alvira-dev
git pull

# 5. Run the test suite on Pi (vosk + pyalsaaudio now present)
python -m pytest tests/ -q --tb=short

# 6. Run an integration smoke test
python - << 'EOF'
from autochair.audio.config import AudioConfig
from autochair.audio.devices.capture import ALSAAudioCapture
from autochair.audio.devices.preprocessor import AudioPreprocessor
from autochair.audio.speech.vosk_recognizer import VoskSpeechRecognizer

config = AudioConfig(vosk_model_path="vosk-model-small-en-us-0.15")
preprocessor = AudioPreprocessor(config)
recognizer = VoskSpeechRecognizer(config)

with ALSAAudioCapture(config) as capture:
    for _ in range(20):                    # ~20 chunks × 85 ms ≈ 1.7 s
        raw = capture.read_chunk()
        pcm = preprocessor.process(raw)
        text = recognizer.recognize(pcm)
        if text:
            print(f"RECOGNIZED: {text}")
EOF
```

---

## Experimental Pi Validation (Pre-Repository)

Before this code was written, the following was validated on the physical
Raspberry Pi 4 in an experimental script:

| Test | Result |
|---|---|
| I2S microphone capture (Phase 2) | VERIFIED — audible voice recording |
| Python ALSA capture | VERIFIED — pyalsaaudio 0.11.0 |
| Audio conversion (S32_LE → S16, 48→16 kHz, mono) | VERIFIED — manual Python script |
| Vosk recognition (restricted grammar) | VERIFIED — "start", "stop", "move forward" recognized |
| Unknown speech | VERIFIED — `[unk]` returned |

**This is an experimental result only.** The full repository implementation
has not yet been deployed and tested on the Pi.  The repository status for
Phase 3 is: **SOFTWARE COMPLETE — PI DEPLOYMENT PENDING**.

---

## Not Yet Completed

- Full Phase 3 Pi deployment and verification
- TTS (text-to-speech) audio output
- PAM8403 amplifier and speaker hardware
- Buzzer (GPIO alert) integration
- Touchscreen voice-status integration
- Wake word detection
- Additional voice commands (backward, left, right, etc.)
- Production safety validation for voice-to-motion
- Real wheelchair motor control through voice

---

## Safety Reminder

Voice recognition in AutoChair must **always** flow through the validated pipeline:

```
VOICE → COMMAND PARSER → INPUT VALIDATION → SAFETY LAYER → MOTION CONTROL
```

Phase 3 does not connect voice commands to wheelchair motors.
All motion remains simulated (logged only).
