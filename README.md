# CH552-JTAG
USB-JTAG Adapter Using CH552

(C) Bo Gao, 2020

MCU C code licensed under BSD 3-clause.

CLI Python code licensed under BSD 3-clause, created based on Gowin TN653 document.

This project aims to provide a low cost USB to JTAG adapter for various chips, in this case, Gowin FPGAs.

The MCU handles low level TMS, TDI, TDO shifting in bitbang mode and TDI shifting in SPI mode.

The Python script uses bitbang mode manipulate TMS, and use SPI mode to shift out the majority of data.

Also supplied is a LED blinker created for Trenz's Gowin GW1NR-LV9 board.

MCU firmware compilation: use the supplied makefile, you will need SDCC installed.

Python script: use usbjtag.py -rcsv -f [filename], you will need PyUSB and libusb installed.

This firmware implemented WCID for WinUSB, but for whatever reason, PyUSB has a problem communicating with it.

Thus, WCID support has been since removed and Zadig is needed to install libusb driver manually on Windows.
