# CH552-JTAG
USB-JTAG Adapter Using CH552

(C) Bo Gao, 2020

MCU C code licensed under BSD 3-clause
Demo Python code is public domain material, created based on Gowin TN653 document.

This project aims to provide a low cost USB to JTAG adapter for various chips, in this case, Gowin FPGAs.
The MCU handles only low level TMS and TDI (for FPGA, TDO for MCU) shifting out and TDO shifting in.
All logic, such as TAP moving, register access and more are implemented in the Python code.
Also supplied is a LED blinker created for Trenz's Gowin GW1NR-LV9 board.

MCU firmware compilation: use the supplied makefile, you will need SDCC installed.
Python script: use jtag.py [binary bitstream filename], you will need PyUSB and libusb installed.

This firmware implements WCID for WinUSB, but for whatever reason, PyUSB has a problem communicating with it.
For Windows users, use Zadig to install libusb-win32 for the device for PyUSB to play well.
