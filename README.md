# BarePD

**Bare Metal Pure Data for Raspberry Pi**

BarePD runs [Pure Data](https://puredata.info/) patches directly on Raspberry Pi hardware without any operating system. This enables ultra-low latency audio synthesis with deterministic timing - perfect for embedded musical instruments, synthesizers, and audio installations.

## Features

- 🎵 **libpd Integration** - Full Pure Data audio engine running bare metal
- ⚡ **Ultra-Low Latency** - No OS overhead, direct hardware access
- 🎹 **USB MIDI Support** - Connect any class-compliant USB MIDI controller
- 💾 **SD Card Patches** - Load `.pd` patches directly from FAT32 SD card
- 🔊 **Multiple Audio Outputs**:
  - **PWM** - Built-in 3.5mm headphone jack
  - **I2S** - External DACs like PCM5102A (high quality!)
  - **USB Audio** - USB audio interfaces (planned)
- 🖥️ **Serial Console** - Debug output via UART

## Supported Hardware

- **Raspberry Pi 3B/3B+** (primary target, 32-bit)
- Raspberry Pi 4 (planned)
- Raspberry Pi Zero 2 W (planned)

## Quick Start

### 1. Download Release

Download the latest release from the [Releases](https://github.com/reverbrick/parepd/releases) page.

### 2. Prepare SD Card

Format an SD card as FAT32 and copy these files:

```
SD Card Root/
├── bootcode.bin      # From Raspberry Pi firmware
├── start.elf         # From Raspberry Pi firmware  
├── fixup.dat         # From Raspberry Pi firmware
├── config.txt        # BarePD configuration
├── cmdline.txt       # Audio settings (optional)
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

### PWM (Default) - 3.5mm Jack

Uses the built-in headphone jack. Good for testing, but lower quality.

**cmdline.txt:**
```
audio=pwm samplerate=48000
```

### I2S DAC - PCM5102A

The popular PCM5102A module provides high-quality audio output. No driver configuration needed!

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

**config.txt:**
```ini
dtparam=i2s=on
```

**cmdline.txt:**
```
audio=i2s samplerate=48000
```

### USB Audio Interface (Planned)

USB audio interface support is planned for a future release.

**Notes:**
- Will support class-compliant USB audio devices
- Multichannel output planned (up to 8 channels)

## Building from Source

### Prerequisites

- macOS, Linux, or Windows with WSL
- ARM GNU Toolchain (arm-none-eabi-gcc)
- Git

### Clone Repository

```bash
git clone --recursive https://github.com/reverbrick/parepd.git
cd parepd
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

The kernel image will be at `src/kernel8-32.img`.

## Configuration Reference

### cmdline.txt Options

| Option | Values | Description |
|--------|--------|-------------|
| `audio` | `pwm`, `i2s` | Audio output type |
| `samplerate` | `44100`, `48000`, `96000` | Sample rate in Hz |

### config.txt Options

```ini
# Required settings
arm_64bit=0
kernel=kernel8-32.img
dtparam=audio=on

# For I2S DAC (PCM5102A)
dtparam=i2s=on

# Memory allocation
gpu_mem=64
```

### Serial Console

Connect a USB-to-serial adapter for debug output:

| Pi GPIO | Serial Adapter |
|---------|----------------|
| GPIO 14 (TXD) | RX |
| GPIO 15 (RXD) | TX |
| GND | GND |

Baud rate: **115200**

## Pure Data Patch Guidelines

### Supported Objects

Most core Pd objects work, including:
- Audio: `osc~`, `phasor~`, `noise~`, `+~`, `*~`, `dac~`, etc.
- Control: `metro`, `counter`, `random`, `select`, etc.
- MIDI: `notein`, `ctlin`, `pgmin`, `bendin`
- Math: `+`, `-`, `*`, `/`, `sin`, `cos`, etc.
- Arrays and tables

### MIDI Input

MIDI messages from USB are automatically routed to Pd. Use standard MIDI objects:

```
[notein]       - Note messages (note, velocity, channel)
[ctlin]        - Control change
[pgmin]        - Program change  
[bendin]       - Pitch bend
```

### Limitations

- No GUI objects (running headless)
- No file I/O beyond patch loading
- No networking objects
- No external libraries (yet)

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                    BarePD Kernel                     │
├─────────────────────────────────────────────────────┤
│  ┌─────────┐  ┌─────────┐  ┌──────────────────────┐ │
│  │  libpd  │  │  USB    │  │   Audio Output       │ │
│  │  (Pd    │◄─│  MIDI   │  │  (PWM/I2S/USB)       │ │
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
parepd/
├── src/                    # BarePD source code
│   ├── kernel.cpp          # Main kernel implementation
│   ├── kernel.h            # Kernel header
│   ├── pdsounddevice.cpp   # Audio drivers (PWM/I2S/USB)
│   ├── pdsounddevice.h     # Sound device header
│   ├── pd_compat.c         # OS compatibility layer
│   ├── main.cpp            # Entry point
│   └── Makefile            # Build configuration
├── circle/                 # Circle bare metal framework (submodule)
├── libpd/                  # libpd library (submodule)
├── sdcard/                 # SD card template files
│   ├── config.txt          # Pi configuration
│   ├── cmdline.txt         # Audio settings
│   └── README.md           # SD card setup guide
├── patches/                # Example Pure Data patches
└── README.md               # This file
```

## Troubleshooting

### No Audio Output

1. Check that `main.pd` exists on SD card
2. Verify patch has `dac~` object connected
3. Try a simple test patch (sine wave)
4. Check serial console for error messages
5. For I2S: verify wiring and config.txt settings
6. For USB: ensure device is plugged in

### I2S (PCM5102A) Not Working

1. Check wiring carefully (BCK, LRCK, DIN pins)
2. Ensure `dtparam=i2s=on` in config.txt
3. Set `audio=i2s` in cmdline.txt
4. Connect XSMT to 3.3V to unmute
5. Connect FMT to GND for I2S mode

### Patch Not Loading

1. Ensure filename is `main.pd` (or any `.pd` file)
2. File must be in root of SD card
3. Check patch size is under 256KB
4. Verify patch syntax in desktop Pd first

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Submit a pull request

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file.

## Credits

- [Circle](https://github.com/rsta2/circle) - C++ bare metal environment for Raspberry Pi
- [libpd](https://github.com/libpd/libpd) - Pure Data as an embeddable library
- [Pure Data](https://puredata.info/) - The open source visual programming language

## Related Projects

- [mt32-pi](https://github.com/dwhinham/mt32-pi) - Bare metal MT-32 emulator (inspiration)
- [MiniSynth Pi](https://github.com/rsta2/minisynth) - Circle-based synthesizer
