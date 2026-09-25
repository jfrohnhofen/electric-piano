MCU := "atmega16"
F_CPU := "16000000"
FORMAT := "ihex"
SERIAL := `ls /dev/tty.usb* 2>/dev/null | head -n 1 || echo "/dev/ttyUSB0"`
CXXDEFS := "-D__AVR_" + MCU + "__ -DF_CPU=" + F_CPU + "UL"
CXXFLAGS := CXXDEFS + " -mmcu=" + MCU + " -Os"
OBJCOPYFLAGS := "-j .text -j .data -O " + FORMAT
PROGFLAGS := "-cstk500v1 -p" + MCU + " -P" + SERIAL + " -b19200"

# --- FIRMWARE ---

# Build both bootloader and firmware
build-firmware: firmware bootloader

# Build the firmware
firmware:
	cd firmware && avr-g++ {{CXXFLAGS}} firmware.cpp -o firmware.obj
	cd firmware && avr-objcopy {{OBJCOPYFLAGS}} firmware.obj firmware.hex

# Build the bootloader
bootloader:
	cd firmware && avr-g++ {{CXXFLAGS}} -nostartfiles -Wl,--relax,--section-start=.text=0x3c00 bootloader.cpp -o bootloader.obj
	cd firmware && avr-objcopy {{OBJCOPYFLAGS}} bootloader.obj bootloader.hex

# Flash the bootloader to the device
flash-bootloader: bootloader
	avrdude {{PROGFLAGS}} -v -U flash:w:firmware/bootloader.hex:i

# Write fuses to the device
write-fuses:
	avrdude {{PROGFLAGS}} -U hfuse:w:0xca:m -U lfuse:w:0xff:m

# Read flash from the device
read-flash:
	avrdude {{PROGFLAGS}} -U flash:r:firmware/flash.bin:r

# Clean firmware build artifacts
clean-firmware:
	rm -f firmware/*.obj firmware/*.hex firmware/*.bin

# --- PROGRAMMER (RUST) ---

# Build the programmer
build-programmer:
	cd programer && cargo build

# Run the programmer
run-programmer:
	cd programer && cargo run

# --- ALL ---

# Build everything
build-all: build-firmware build-programmer

# Clean everything
clean: clean-firmware
	cd programer && cargo clean
