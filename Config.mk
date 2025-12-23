#
# Config.mk - BarePD for Raspberry Pi 3B (32-bit)
#

RASPPI = 3
AARCH = 32

# Use ARM's official toolchain (GCC 13.2) - Homebrew's GCC 15 breaks bare-metal
PREFIX = /Users/daniel/Documents/BarePD/arm-gnu-toolchain-13.2.Rel1-darwin-arm64-arm-none-eabi/bin/arm-none-eabi-

# Enable standard library support
STDLIB_SUPPORT = 3

# Optimization
OPTIMIZE = -O2
