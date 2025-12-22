# BarePD SD Card Setup

This folder contains template files for creating a bootable BarePD SD card.

## Required Files

Copy these files to a **FAT32-formatted** SD card:

### From This Folder
- `kernel8-32.img` - The BarePD kernel
- `config.txt` - Raspberry Pi boot configuration

### From Raspberry Pi Firmware
Download from https://github.com/raspberrypi/firmware/tree/master/boot:
- `bootcode.bin` - First stage bootloader
- `start.elf` - GPU firmware
- `fixup.dat` - Memory fixup file

### Your Pure Data Patch
- `main.pd` - Your Pure Data patch (or any `.pd` file)

## Final SD Card Structure

```
SD Card (FAT32)/
├── bootcode.bin     ← from Pi firmware
├── start.elf        ← from Pi firmware
├── fixup.dat        ← from Pi firmware
├── config.txt       ← from this folder
├── kernel8-32.img   ← from this folder
└── main.pd          ← your patch
```

## Audio Output Options

### Default: PWM Audio (3.5mm Jack)
Works out of the box. Just plug headphones or speakers into the Pi's audio jack.

### I2S DAC (Better Quality)
For external I2S DACs like HifiBerry, edit `config.txt`:
```ini
dtparam=i2s=on
dtoverlay=hifiberry-dac
```

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

**Boots but no audio:**
- Check `main.pd` exists and has `dac~` connected
- Try the example `sinewave.pd` patch
- Connect serial console for debug output

**MIDI not working:**
- Plug in MIDI device after Pi boots
- Only class-compliant USB MIDI supported
