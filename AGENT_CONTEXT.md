# BarePD Agent Context

> **Last Updated:** 2024-12-23 (FUDI remote control, headless mode, low latency)
> **Purpose:** Quick context recovery for AI agents working on this project

## Project Overview

BarePD is a **bare metal Pure Data implementation** for Raspberry Pi using the Circle framework. It runs Pd patches directly on hardware without an OS for ultra-low latency audio.

## Current Configuration

| Setting | Value |
|---------|-------|
| Audio Output | **I2S** (PCM5102A DAC) |
| Sample Rate | 48000 Hz |
| Target | Raspberry Pi 3B (32-bit) |
| Kernel | `kernel8-32.img` |

## Key Files

| File | Purpose |
|------|---------|
| `src/kernel.cpp` | Main kernel, initialization, main loop |
| `src/kernel.h` | Kernel class with `CPdSoundI2S *m_pI2SDevice` member |
| `src/pdsounddevice.cpp` | Audio drivers (CPdSoundPWM, CPdSoundI2S) |
| `src/pdsounddevice.h` | Audio device classes and factory |
| `src/pd_fileio.cpp` | File I/O bridge for libpd → Circle filesystem |
| `src/pd_compat.c` | POSIX compatibility shims for libpd |

## Recent Changes (Dec 2024)

1. **Switched from PWM to I2S audio** - PCM5102A DAC support
2. **Low latency optimization** - ~50ms audio latency:
   - Queue: 50ms buffer
   - Chunk: 256 frames (~5ms)
   - No sleep in main loop
3. **Headless mode** - `headless=1` in cmdline.txt disables video
4. **Fast boot** - `disable_splash=1` removes rainbow screen
5. **Removed all logging in audio loop** - No stuttering from debug output
6. **FUDI remote control** - Control patches via USB CDC or UART serial
   - USB: Pi appears as /dev/ttyACM0 on host
   - UART: GPIO 14/15 at 115200 baud
   - Bidirectional: Pd [s name] sends back to serial
   - Files: pd_fudi.h/cpp, uses CUSBCDCGadget

## I2S Audio Parameters (Low Latency)

```cpp
// In pdsounddevice.cpp - optimized for ~50ms latency
#define I2S_CHUNK_SIZE     256         // ~5ms chunks
#define I2S_QUEUE_SIZE_MS  50          // 50ms buffer

// In kernel.cpp main loop - no sleep for lowest latency
m_pI2SDevice->Process();  // Continuous queue filling
```

## Fast Boot config.txt Options

```ini
disable_splash=1          # Remove rainbow screen
boot_delay=0              # No boot delay
force_turbo=1             # CPU always at max
gpu_mem=16                # Minimal GPU memory
dtparam=act_led_trigger=none
```

## Build Commands

```bash
cd /Users/daniel/Documents/BarePD/src
make clean && make
# Output: kernel8-32.img
```

## SD Card Setup

Required files on FAT32 SD card:
- `bootcode.bin`, `start.elf`, `fixup.dat` (RPi firmware)
- `kernel8-32.img` (BarePD kernel)
- `config.txt` (with `kernel=kernel8-32.img`, `dtparam=i2s=on`)
- `cmdline.txt` (with `audio=i2s samplerate=48000`)
- `main.pd` (Pure Data patch)
- Optional: `.wav` files for sample playback

## PCM5102A Wiring

| PCM5102A | Raspberry Pi |
|----------|--------------|
| VIN | 3.3V (Pin 1) |
| GND | GND (Pin 6) |
| BCK | GPIO 18 (Pin 12) |
| LRCK | GPIO 19 (Pin 35) |
| DIN | GPIO 21 (Pin 40) |
| FMT | GND |
| XSMT | 3.3V |

## Known Limitations

- Circle's FAT filesystem: root directory only (no subdirectories)
- No GUI objects in patches (headless)
- File I/O limited to patch loading and soundfiler
- Max patch size: 256KB

## Useful Paths

```
Project:     /Users/daniel/Documents/BarePD
Source:      /Users/daniel/Documents/BarePD/src
Circle:      /Users/daniel/Documents/BarePD/circle
libpd:       /Users/daniel/Documents/BarePD/libpd
Patches:     /Users/daniel/Documents/BarePD/patches
SD template: /Users/daniel/Documents/BarePD/sdcard
```

## Quick Recovery Commands

```bash
# Check current audio config
grep -E "CHUNK_SIZE|QUEUE_SIZE" src/pdsounddevice.cpp

# Check main loop timing
grep -A5 "AudioOutputI2S && m_pI2SDevice" src/kernel.cpp

# Build and copy to SD
cd src && make && cp kernel8-32.img /Volumes/BAREPD/
```

---

*To update this file: Ask the agent to "update AGENT_CONTEXT.md with [changes]"*

