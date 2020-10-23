import usb
import binascii
import struct

dev=usb.core.find(idVendor=0x20a0, idProduct=0x4209)

if dev is None:
    raise ValueError("Device not found")

dev.set_configuration()

try: # Connection will break
    dev.write(0x01, [0xff])
except Exception:
    pass
