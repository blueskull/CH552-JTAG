CFLAGS=-mmcs51 --model-small --xram-size 0x0400 --xram-loc 0x0000 --code-size 0x3800

all:
	sdcc $(CFLAGS) usbjtag.c -o usbjtag.hex
	objcopy -I ihex -O binary usbjtag.hex usbjtag.bin
	-@rm -rf *.asm *.lst *.rel *.rst *.sym *.lk *.map *.mem *.hex

flash:
	-@python3 usbjtag.py -i
	python3 ch55xtool.py -r -f usbjtag.bin

test:
	python3 usbjtag.py -rcsv -f test.bin
