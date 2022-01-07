all:
	sdcc -mmcs51 --xram-size 0x0400 --xram-loc 0x0000 --code-size 0x3800 usbjtag.c -o usbjtag.hex
	objcopy -I ihex -O binary usbjtag.hex usbjtag.bin
	-@rm -rf *.asm *.lst *.rel *.rst *.sym *.lk *.map *.mem *.hex

flash: all
	./cli.js write mcu usbjtag.bin