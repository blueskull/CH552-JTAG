import usb
import binascii
import struct
import time
import sys
dev=usb.core.find(idVendor=0x20a0, idProduct=0x4209)

if dev is None:
	raise ValueError("Device not found")

dev.set_configuration()

# Reset FPGA
dev.write(0x01, [0x00, 0x00])
dev.write(0x01, [0x00, 0x02])

# Reset JTAG TAP
dev.write(0x01, [0x01, 2, 0, 0x00, 0xff, 0x00, 0x00, 0x00])

def jtag_inst(inst):
    dev.write(0x01, [0x01, 3, 0, 0, 0x30, 0x80, 0x01, 0x00, inst&0xff, 0x00])

def jtag_read():
    dev.write(0x01, [0x01, 6, 0, 0, 0x20, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
    ret=dev.read(0x81, 64, 100)
    ret.reverse()
    return bytes(ret[1:5]).hex()

# Read ID
jtag_inst(0x11)
print("Device ID is: "+jtag_read())

# Read status
# jtag_inst(0x41)
# print("Device status is: "+jtag_read())

# Clear SRAM
jtag_inst(0x15)
jtag_inst(0x05)
jtag_inst(0x02)
time.sleep(0.01)
jtag_inst(0x3a)
jtag_inst(0x02)

print("FPGA cleared.")

# Write SRAM
jtag_inst(0x15)
jtag_inst(0x17)
dev.write(0x01, [0x01, 1, 0, 0, 0x20, 0x00]) # Shift DR
dev.write(0x01, [0x00, 0x06]) # Enter SPI mode
with open(sys.argv[1], "rb") as f:
    while True:
        dat=[0]*62 # 62 bytes at a time
        for i in range(62):
            ba=f.read(1)
            if not ba:
                break
            dat[i]=ba[0]
        i=i+1;
        dev.write(0x01, [0x02, 0x00]+dat[0:i]) # No NCS toggling
        if i!=62:
            break
dev.write(0x01, [0x00, 0x02]) # Exit SPI mode
dev.write(0x01, [0x01, 2, 0, 0, 0x80, 0x01, 0x00, 0x00]) # Idle
jtag_inst(0x3a)
jtag_inst(0x02)

print("Configuration done.")
