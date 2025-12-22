# BarePD SD Card Setup

This folder contains template files for creating a bootable BarePD SD card.

## Required Files

Copy these files to a **FAT32-formatted** SD card:

### From This Folder
- `kernel8-32.img` - The BarePD kernel
- `config.txt` - Raspberry Pi boot configuration
- `cmdline.txt` - Audio configuration (optional)
- `main.pd` - Example Pure Data patch

### From Raspberry Pi Firmware
Download from https://github.com/raspberrypi/firmware/tree/master/boot:
- `bootcode.bin` - First stage bootloader
- `start.elf` - GPU firmware
- `fixup.dat` - Memory fixup file

## Final SD Card Structure

```
SD Card (FAT32)/
├── bootcode.bin     ← from Pi firmware
├── start.elf        ← from Pi firmware
├── fixup.dat        ← from Pi firmware
├── config.txt       ← from this folder
├── cmdline.txt      ← from this folder (audio settings)
├── kernel8-32.img   ← from this folder
└── main.pd          ← your patch (or from this folder)
```

## Audio Output Options

### PWM Audio (Default) - 3.5mm Jack

Works out of the box - plug headphones/speakers into the Pi's audio jack.

**cmdline.txt:**
```
audio=pwm samplerate=48000
```

### I2S DAC - PCM5102A Module

The PCM5102A is a popular high-quality I2S DAC module (~$5).

**cmdline.txt:**
```
audio=i2s samplerate=48000
```

**config.txt** (add this line):
```
dtparam=i2s=on
```

**Wiring PCM5102A to Raspberry Pi 3B:**

```
PCM5102A Module          Raspberry Pi 3B
┌─────────────┐         ┌─────────────────┐
│  VIN   ●────┼─────────┤ Pin 1  (3.3V)   │
│  GND   ●────┼─────────┤ Pin 6  (GND)    │
│  BCK   ●────┼─────────┤ Pin 12 (GPIO18) │
│  LRCK  ●────┼─────────┤ Pin 35 (GPIO19) │
│  DIN   ●────┼─────────┤ Pin 40 (GPIO21) │
│  FMT   ●────┼─────────┤ GND (I2S mode)  │
│  XSMT  ●────┼─────────┤ 3.3V (unmute)   │
└─────────────┘         └─────────────────┘
```

**Notes:**
- FMT to GND = I2S format (default for most audio)
- XSMT to 3.3V = Soft unmute (required for audio output)
- Some modules have FLT and DEMP pins - leave unconnected

## Serial Console (Debug)

Connect a USB-to-serial adapter:
- GPIO 14 (TXD) → RX on adapter
- GPIO 15 (RXD) → TX on adapter  
- GND → GND

Settings: **115200 baud, 8N1**

## Troubleshooting

**No boot activity (no green LED flashing):**
- Check SD card is FAT32 formatted
- Verify all firmware files are present
- Try a different SD card

**PWM audio but no I2S:**
- Check `dtparam=i2s=on` in config.txt
- Check `audio=i2s` in cmdline.txt
- Verify PCM5102A wiring
- Connect XSMT to 3.3V (unmute!)

**Boots but no audio:**
- Check `main.pd` exists and has `dac~` connected
- Try the example `sinewave.pd` patch
- Connect serial console for debug output

**MIDI not working:**
- Plug in MIDI device after Pi boots
- Only class-compliant USB MIDI supported
