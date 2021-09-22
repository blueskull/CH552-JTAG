# CH552-JTAG
USB-JTAG Adapter Using CH552

(C) Bo Gao, 2020~2021

MCU C code and PC Python code licensed under BSD 3-clause.

Python code usbjtag.py licensed under BSD 3-clause, created based on Gowin TN653 document (discontinued, now merged into UG290).

This project aims to provide a low cost USB to JTAG adapter for various chips, in this case, Gowin FPGAs.

The MCU handles low level TMS, TDI, TDO shifting in bitbang mode and TDI shifting in SPI mode.

Double buffering is added to smooth command executions, so that precise delays as well as continuous JTAG clock pluses can be added between operations.

This is needed for certain timing-critical and clock-critical operations, such as flash programming for certain Gowin parts.

The Python script uses bitbang mode manipulate TMS, and use SPI mode to shift out the majority of data.

THe Python script does not support flash programming yet, but support for GW1NZ will be added shortly.

Also supplied is a LED blinker created for Trenz's Gowin GW1NR-LV9 board.

MCU firmware compilation: use the supplied makefile, you will need SDCC installed.

Python script: refer to the built-in help (./usbjtag.py -h), you will need PyUSB and libusb installed.

This firmware implemented WCID for WinUSB, but for whatever reason, PyUSB has a problem communicating with it.

Thus, WCID support has been since removed and Zadig is needed to install libusb driver manually on Windows.

The current version has NOT been tested on Windows or macOS, and is only tested on Linux 5.11. Use at your own risk on untested platforms.
