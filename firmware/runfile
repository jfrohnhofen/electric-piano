MCU = atmega16
F_CPU = 16000000

FORMAT = ihex
SERIAL = /dev/$(shell ls /dev | grep tty.usb)

CXXDEFS = -D__AVR_$(MCU)__ -DF_CPU=$(F_CPU)UL
CXXFLAGS += $(CXXDEFS) -mmcu=$(MCU) -Os

OBJCOPYFLAGS = -j .text -j .data -O $(FORMAT)

PROGFLAGS = -cstk500v1 -p$(MCU) -P$(SERIAL) -b19200

bootloader:
	avr-g++ $(CXXFLAGS) -nostartfiles -Wl,--relax,--section-start=.text=0x3c00 bootloader.cpp -o bootloader.obj
	avr-objcopy $(OBJCOPYFLAGS) bootloader.obj bootloader.hex

firmware:
	avr-g++ $(CXXFLAGS) firmware.cpp -o firmware.obj
	avr-objcopy $(OBJCOPYFLAGS) firmware.obj firmware.hex

flash: bootloader
	avrdude $(PROGFLAGS) -v -U flash:w:bootloader.hex:i

fuses:
	avrdude $(PROGFLAGS) -U hfuse:w:0xca:m -U lfuse:w:0xff:m

read-flash:
	avrdude $(PROGFLAGS) -U flash:r:flash.bin:r

clean:
	rm *.obj *.hex
