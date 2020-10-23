CFLAGS=-mmcs51 --model-small --xram-size 0x0400 --xram-loc 0x0000 --code-size 0x3800

all:
	sdcc $(CFLAGS) -c usb.c -o usb.rel
	sdcc $(CFLAGS) -c jtag.c -o jtag.rel
	sdcc $(CFLAGS) -c main.c -o main.rel
	sdcc $(CFLAGS) usb.rel jtag.rel main.rel -o output.hex
	@rm -rf *.asm *.lst *.rel *.rst *.sym *.lk *.map *.mem

clean:
	@rm -rf *.asm *.lst *.rel *.rst *.sym *.lk *.map *.mem *.ihx
