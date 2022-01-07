# CH552-JTAG
USB-JTAG Adapter Using CH552

(C) Bo Gao, 2020~2022

MCU C code and PC JaveScript code licensed under BSD 3-clause.

JavaScript code usbjtag.js and cli.js licensed under BSD 3-clause, created based on Gowin TN653 document (discontinued, now merged into UG290).

This project aims to provide a low cost USB to JTAG adapter for various chips, in this case, Gowin FPGAs.

The MCU handles low level TMS, TDI, TDO shifting in bitbang mode and TDI shifting in SPI mode.

Double buffering is added to accelerate data transfer, as well as programmable and pin-selectable clock output.

The above are needed for certain timing-critical and clock-critical operations, such as flash programming for certain Gowin parts.

MCU firmware compilation: use the supplied makefile, you will need SDCC installed.

JavaScript CLI: refer to the built-in help "./cli.js", you will need libusb and a few Node.JS packages installed, run "npm i" to install all packages.

Zadig is needed to install libusb driver manually on Windows. Look for USB-JTA device and install libusb for it.

UDEV rules need to be added to grant non-root users access to the device.

The current version has NOT been tested on Windows or macOS, and is only tested on Linux 5.11. Use at your own risk on untested platforms.