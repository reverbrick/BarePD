# BarePD

**Bare Metal Pure Data for Raspberry Pi** ✅ *Working!*

*Created by Daniel Górny — [PlayableElectronics](https://rlrl.pl/)*

BarePD runs [Pure Data](https://puredata.info/) patches directly on Raspberry Pi hardware without any operating system. This enables ultra-low latency audio synthesis with deterministic timing - perfect for embedded musical instruments, synthesizers, and audio installations.

> **Status:** Tested and working on Raspberry Pi 3B+ with I2S audio (PCM5102A DAC) and PWM (3.5mm jack). ~50ms audio latency achieved!

## Features

- 🎵 **libpd Integration** - Full Pure Data audio engine running bare metal
- ⚡ **Ultra-Low Latency** - ~50ms with optimized settings, no OS overhead
- 🎹 **USB MIDI Support** - Connect any class-compliant USB MIDI controller
- 💾 **SD Card Patches** - Load `.pd` patches directly from FAT32 SD card
- 🔊 **Multiple Audio Outputs**:
  - **I2S** - External DACs like PCM5102A (recommended!)
  - **PWM** - Built-in 3.5mm headphone jack
  - **USB Audio** - USB audio interfaces (planned)
- 🚀 **Fast Boot** - Under 3 seconds to audio with optimized config
- 👁️ **Headless Mode** - Disable video for maximum performance
- 📡 **FUDI Remote Control** - Control patches via USB or UART serial
- 🖥️ **Serial Console** - Debug output via UART

## Supported Hardware

- **Raspberry Pi 3B/3B+** (primary target, 32-bit) → `kernel8-32.img`
- **Raspberry Pi Zero W** (single-core, tested) → `kernel.img`
- Raspberry Pi Zero 2 W (should work with `kernel8-32.img`, untested)
- Raspberry Pi 4 (planned)

## Quick Start

### 1. Download Release

Download the latest release from the [Releases](https://github.com/reverbrick/BarePD/releases) page.

### 2. Prepare SD Card

Format an SD card as FAT32 and copy these files:

```
SD Card Root/
├── bootcode.bin      # From Raspberry Pi firmware
├── start.elf         # From Raspberry Pi firmware  
├── fixup.dat         # From Raspberry Pi firmware
├── config.txt        # BarePD configuration
├── cmdline.txt       # Audio settings
├── kernel8-32.img    # BarePD kernel
└── main.pd           # Your Pure Data patch
```

Get the firmware files from: https://github.com/raspberrypi/firmware/tree/master/boot

### 3. Create a Patch

Create a Pure Data patch named `main.pd`. Here's a simple example:

```
#N canvas 0 0 450 300 10;
#X obj 50 50 osc~ 440;
#X obj 50 100 *~ 0.1;
#X obj 50 150 dac~;
#X connect 0 0 1 0;
#X connect 1 0 2 0;
#X connect 1 0 2 1;
```

### 4. Boot

Insert the SD card into your Raspberry Pi and power on. Audio will start playing through the selected output.

## Audio Output Options

### I2S DAC - PCM5102A (Recommended)

The PCM5102A module provides high-quality, low-latency audio output.

**Wiring:**

| PCM5102A | Raspberry Pi |
|----------|--------------|
| VIN      | 3.3V (Pin 1) |
| GND      | GND (Pin 6)  |
| BCK      | GPIO 18 (Pin 12) |
| LRCK     | GPIO 19 (Pin 35) |
| DIN      | GPIO 21 (Pin 40) |
| FMT      | GND (I2S mode) |
| XSMT     | 3.3V (unmute) |

**cmdline.txt:**
```
audio=i2s samplerate=48000
```

**config.txt:**
```ini
dtparam=i2s=on
```

### PWM - 3.5mm Jack

Uses the built-in headphone jack. Lower quality but no extra hardware needed.

**cmdline.txt:**
```
audio=pwm samplerate=48000
```

## Performance Optimization

### Headless Mode (Recommended)

Disable video output to free resources for audio processing:

**cmdline.txt:**
```
audio=i2s samplerate=48000 headless=1
```

In headless mode:
- Video is completely disabled
- Logging goes to serial console only
- More CPU/memory available for audio
- Connect USB-serial adapter to see boot messages

### Fast Boot

Add these to `config.txt` for faster startup:

```ini
disable_splash=1          # Remove rainbow screen
boot_delay=0              # No boot delay  
initial_turbo=1           # CPU turbo during boot
```

### Low Latency Audio

Current optimized settings (~50ms latency):
- I2S queue: 50ms buffer
- Chunk size: 256 frames (~5ms)
- No logging during audio processing

## FUDI Remote Control

BarePD supports the FUDI (Fast Universal Digital Interface) protocol for remote control via serial. This allows you to:
- Control patch parameters from a computer
- Build hardware controllers (Arduino, ESP32)
- Create remote displays showing Pd output

### Connection Options

**USB Serial (Pi appears as /dev/ttyACM0 on host):**
```bash
# Linux/macOS
screen /dev/ttyACM0 115200

# Or use any serial terminal
```

**UART Serial (GPIO pins):**
- TX: GPIO 14 (Pin 8)
- RX: GPIO 15 (Pin 10)
- Baud: 115200

### FUDI Protocol

Messages are ASCII text terminated by semicolon:

```
receiver value;
```

**Examples:**
```bash
# Send float to [r freq]
freq 440;

# Send bang to [r trigger]  
trigger bang;

# Send symbol to [r mode]
mode sine;

# Control DSP
pd dsp 1;
pd dsp 0;
```

### Bidirectional Communication

Messages sent from Pd via `[s name]` are output as FUDI:

```
# In Pd patch:
[s output]  <- sending 123 outputs: "output 123;"

# Received on serial:
output 123;
```

### Example Patch for FUDI

```
[r freq]     <- receives "freq 440;"
|
[osc~]
|
[*~]
|        [r amp] <- receives "amp 0.5;"
|        |
|        [/ 100]
|        |
[*~      ]
|
[dac~]
```

### Disable FUDI

If not needed, disable to save resources:

```
fudi=0
```

## Configuration Reference

### cmdline.txt Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `audio` | `i2s`, `pwm` | `i2s` | Audio output type |
| `samplerate` | `44100`, `48000`, `96000` | `48000` | Sample rate in Hz |
| `headless` | `0`, `1` | `0` | Disable video output |
| `fudi` | `0`, `1` | `1` | Enable FUDI serial control |

### config.txt Options

```ini
# Required settings
arm_64bit=0
kernel=kernel8-32.img

# Audio
dtparam=audio=on
dtparam=i2s=on            # For I2S DAC

# Performance
disable_splash=1          # Fast boot
boot_delay=0
initial_turbo=1
gpu_mem=64

# Display
disable_overscan=1
```

### Serial Console

Connect a USB-to-serial adapter for debug output (required in headless mode):

| Pi GPIO | Serial Adapter |
|---------|----------------|
| GPIO 14 (TXD) | RX |
| GPIO 15 (RXD) | TX |
| GND | GND |

Baud rate: **115200**

## Building from Source

### Prerequisites

- macOS, Linux, or Windows with WSL
- ARM GNU Toolchain (arm-none-eabi-gcc)
- Git

### Clone Repository

```bash
git clone --recursive https://github.com/reverbrick/BarePD.git
cd BarePD
```

### Download Toolchain (macOS ARM64)

```bash
curl -L -o arm-toolchain.tar.xz \
  "https://developer.arm.com/-/media/Files/downloads/gnu/13.2.rel1/binrel/arm-gnu-toolchain-13.2.rel1-darwin-arm64-arm-none-eabi.tar.xz"
tar -xf arm-toolchain.tar.xz
```

### Configure and Build

```bash
# Configure Circle for RPi 3B
cd circle
./configure -r 3 -p ../arm-gnu-toolchain-13.2.Rel1-darwin-arm64-arm-none-eabi/bin/arm-none-eabi-

# Add stdlib support
echo "STDLIB_SUPPORT = 3" >> Config.mk

# Build Circle libraries
./makeall
cd addon/SDCard && make && cd ../..

# Build BarePD
cd ../src
make
```

Output: `src/kernel8-32.img`

## Pure Data Patch Guidelines

### Supported Objects

Most core Pd objects work, including:
- Audio: `osc~`, `phasor~`, `noise~`, `+~`, `*~`, `dac~`, `tabplay~`, `tabread4~`
- Control: `metro`, `counter`, `random`, `select`, `loadbang`
- MIDI: `notein`, `ctlin`, `pgmin`, `bendin`
- Math: `+`, `-`, `*`, `/`, `sin`, `cos`, etc.
- Arrays: `table`, `soundfiler` (for sample playback)

### MIDI Input

MIDI messages from USB are automatically routed to Pd:

```
[notein]       - Note messages (note, velocity, channel)
[ctlin]        - Control change
[pgmin]        - Program change  
[bendin]       - Pitch bend
```

### Sample Playback

Load WAV files with `soundfiler`:

```
[loadbang]
|
[read -resize sample.wav array1]
|
[soundfiler]
```

### Limitations

- No GUI objects (running headless)
- Root directory only (no subdirectories on SD card)
- Max patch size: 256KB
- No networking objects
- No external libraries (yet)

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                    BarePD Kernel                     │
├─────────────────────────────────────────────────────┤
│  ┌─────────┐  ┌─────────┐  ┌──────────────────────┐ │
│  │  libpd  │  │  USB    │  │   Audio Output       │ │
│  │  (Pd    │◄─│  MIDI   │  │  (I2S/PWM)           │ │
│  │  Core)  │  │  Input  │  └──────────────────────┘ │
│  └────┬────┘  └─────────┘           ▲               │
│       │                             │               │
│       └─────────Audio Samples───────┘               │
├─────────────────────────────────────────────────────┤
│                Circle Framework                      │
│    (Interrupt, Timer, USB, I2C, SD Card, etc.)      │
├─────────────────────────────────────────────────────┤
│              Raspberry Pi Hardware                   │
└─────────────────────────────────────────────────────┘
```

## Project Structure

```
BarePD/
├── src/                    # BarePD source code
│   ├── kernel.cpp          # Main kernel implementation
│   ├── kernel.h            # Kernel header
│   ├── pdsounddevice.cpp   # Audio drivers (PWM/I2S)
│   ├── pdsounddevice.h     # Sound device classes
│   ├── pd_fileio.cpp       # File I/O bridge for libpd
│   ├── pd_compat.c         # POSIX compatibility layer
│   ├── main.cpp            # Entry point
│   └── Makefile            # Build configuration
├── circle/                 # Circle bare metal framework (submodule)
├── libpd/                  # libpd library (submodule)
├── sdcard/                 # SD card template files
├── patches/                # Example Pure Data patches
└── README.md               # This file
```

## Troubleshooting

### No Audio Output

1. Check that `main.pd` exists on SD card root
2. Verify patch has `dac~` object connected
3. Try a simple test patch (sine wave)
4. Check serial console for error messages
5. For I2S: verify wiring and config.txt settings

### Audio Stuttering

1. Enable headless mode: `headless=1` in cmdline.txt
2. Use I2S instead of PWM for better performance
3. Simplify your patch if CPU usage is too high
4. Check serial output for errors

### I2S (PCM5102A) Not Working

1. Check wiring carefully (BCK, LRCK, DIN pins)
2. Ensure `dtparam=i2s=on` in config.txt
3. Set `audio=i2s` in cmdline.txt
4. Connect XSMT to 3.3V to unmute
5. Connect FMT to GND for I2S mode

### Won't Boot (LED Blinks)

- **4 blinks**: `start.elf` issue - check `gpu_mem` is at least 64
- **7 blinks**: `kernel*.img` not found - check filename matches config.txt

### Patch Not Loading

1. Ensure filename is `main.pd`
2. File must be in root of SD card (not subdirectory)
3. Check patch size is under 256KB
4. Verify patch syntax in desktop Pd first

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Submit a pull request

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file.

## Author

**Daniel Górny** — [PlayableElectronics](https://rlrl.pl/)

## Credits

- [Circle](https://github.com/rsta2/circle) - C++ bare metal environment for Raspberry Pi by Rene Stange
- [libpd](https://github.com/libpd/libpd) - Pure Data as an embeddable library
- [Pure Data](https://puredata.info/) - The open source visual programming language by Miller Puckette

## Related Projects

- [mt32-pi](https://github.com/dwhinham/mt32-pi) - Bare metal MT-32 emulator
- [MiniSynth Pi](https://github.com/rsta2/minisynth) - Circle-based synthesizer
