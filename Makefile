CFLAGS=-mmcs51 --stack-auto
LDFLAGS=-mmcs51 --xram-size 0x0400 --xram-loc 0x0000 --code-size 0x3800

all:
	$(foreach f, $(wildcard *.c), sdcc $(CFLAGS) -c $(f);)
	sdcc $(LDFLAGS) *.rel -o usbjtag.hex
	objcopy -I ihex -O binary usbjtag.hex usbjtag.bin
	-@rm -rf *.asm *.lst *.rel *.rst *.sym *.lk *.map *.mem *.hex

flash: all
	-@python3 usbjtag.py -misp
	python3 -m ch55xtool -r -f usbjtag.bin

test:
	python3 usbjtag.py -s test.bin
