import usb
import binascii
import struct

dev=usb.core.find(idVendor=0x20a0, idProduct=0x4209)

if dev is None:
    raise ValueError("Device not found")

dev.set_configuration()

dev.write(0x01, [0x00, 0x04]) # Enter UART mode
dev.write(0x01, struct.pack("bb5s", 0x02, 0x01, b"Hello"))
