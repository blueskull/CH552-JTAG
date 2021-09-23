#!/usr/bin/env python3

# Copyright (c) 2020-2021, Bo Gao <7zlaser@gmail.com>

# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:

# 1. Redistributions of source code must retain the above copyright notice, this list of
#    conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
#    of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. Neither the name of the copyright holder nor the names of its contributors may be
#    used to endorse or promote products derived from this software without specific
#    prior written permission.

# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
# SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THE
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import sys, usb, time, binascii, enum, os, argparse

class GowinJTAG:
    # USB device handle
    dev=None

    # Open USB device
    def __init__(self):
        self.dev=usb.core.find(idVendor=0x20a0, idProduct=0x4209)
        if self.dev is None:
            print("Error: device not found.")
            sys.exit(-1)
        self.dev.reset()
        self.dev.set_configuration()

    # Close USB device
    def __del__(self):
        try: usb.util.dispose_resources(self.dev)
        except Exception: pass

    # Write to device
    def usb_write(self, cmd, arg, dat=[]):
        self.dev.write(0x01, [cmd, arg]+dat)

    # Read from device
    def usb_read(self):
        return self.dev.read(0x82, 64, 100)
        #try: return self.dev.read(0x82, 64, 100)
        #except Exception: return

    # Write and read
    def usb_call(self, cmd, arg, dat=[]):
        self.usb_write(cmd, arg, dat)
        return self.usb_read()

    # Reset FPGA
    def fpga_rst(self):
        self.usb_write(0x00, 0x00)

    # Read control bytes
    def read_control(self, index):
        ret=self.usb_call(0x00, 0x01, [index&0xff])
        if not ret or len(ret)!=1: return
        return ret

    # Write control bytes
    def write_control(self, index, value):
        self.usb_write(0x00, 0x02, [index&0xff, value&0xff])
    
    # Read bus voltage
    def read_vbus(self):
        ret=self.usb_call(0x00, 0x03)
        if not ret or len(ret)!=1: return
        return ret[0]*0.0258

    # Delay milliseconds and microseconds
    def mcu_delay(self, ms, us):
        self.usb_write(0x00, 0x04, [ms&0xff, us&0xff])

    # Read error status
    def read_status(self):
        ret=self.usb_call(0x00, 0x05)
        if not ret or len(ret)!=1: return 1
        return ret[0]

    # Read serial number
    def read_sn(self):
        ret=self.usb_call(0x00, 0xfd)
        if not ret or len(ret)!=16: return
        return "".join(map(chr, ret))
    
    # Write serial number
    def write_sn(self, sn):
        if len(sn)!=16: return
        self.usb_write(0x00, 0xfe, [ord(c) for c in sn])

    # Reset MCU into ISP mode
    def mcu_isp(self):
        try: self.usb_write(0x00, 0xff)
        except Exception: pass

    # JTA write
    def jtag_write(self, tms, tdi, fast=False):
        if not fast: # JTAG mode
            if len(tms)<1 or len(tms)>31 or len(tdi)!=len(tms): return
            self.usb_write(0x01, 0x00, tms+tdi)
        else: # SPI mode
            if len(tdi)<1 or len(tdi)>62: return
            self.usb_write(0x01, 0x02, tdi)

    # JTAG write and read
    def jtag_write_read(self, tms, tdi, fast=False):
        if not fast: # JTAG mode
            if len(tms)<1 or len(tms)>31 or len(tdi)!=len(tms): return
            self.usb_write(0x01, 0x01, tms+tdi)
        else: # SPI mode
            if len(tdi)<1 or len(tdi)>62: return
            self.usb_write(0x01, 0x03, tdi)
        ret=self.usb_read()
        if not ret or len(ret)!=len(tdi): return
        return ret
    
    # SPI write
    def spi_write(self, tdi):
        if len(tdi)<1 or len(tdi)>62: return
        self.usb_write(0x02, 0x00, tdi)

    # SPI write and read
    def spi_write_read(self, tdi):
        if len(tdi)<1 or len(tdi)>62: return
        self.usb_write(0x02, 0x01, tdi)
        ret=self.usb_read()
        if not ret or len(ret)!=len(tdi): return
        return ret
    
    # Reset TAP
    def jtag_rst(self):
        self.jtag_write([0xff], [0x00])
    
    # Move TAP to run-test-idle
    def jtag_rti(self, count=0):
        if count==0: self.jtag_write([0x00], [0x00])
        while count:
            n=count if count<31 else 31
            tms=[0x00]*n
            tdi=[0x00]*n
            self.jtag_write(tms, tdi)
            count=count-n
    
    # Send a JTAG command
    def jtag_command(self, cmd):
        self.jtag_write([0x30, 0x80, 0x01], [0x00, cmd&0xff, 0x00])
    
    # Read a 32-bit JTAG register
    def jtag_read(self):
        ret=self.jtag_write_read([0x20, 0x00, 0x00, 0x00, 0x80, 0x01], [0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
        if not ret or len(ret)!=6: return
        ret.reverse()
        return bytes(ret[1:5])
    
    # Read FPGA ID
    def fpga_id(self):
        self.jtag_command(0x11)
        ret=self.jtag_read()
        if not ret: return
        return ret.hex()
    
    # Read FPGA configuration status
    def fpga_done(self):
        self.jtag_command(0x41)
        ret=self.jtag_read()
        if not ret: return False
        return True if ret[1]&0x20 else False
    
    # Clear FPGA SRAM
    def fpga_clear(self):
        self.jtag_command(0x15)
        self.jtag_command(0x05)
        self.jtag_command(0x02)
        self.mcu_delay(10, 0)
        self.jtag_command(0x3a)
        self.jtag_command(0x02)
    
    # Program FPGA SRAM
    def fpga_sram(self, filename):
        flen=0
        self.jtag_command(0x15)
        self.jtag_command(0x17)
        self.jtag_write([0x20], [0x00], False) # Shift DR
        with open(filename, "rb") as f:
            while True:
                dat=f.read(62)
                if not dat: break
                self.jtag_write([], list(dat), True)
                flen=f.tell()
        self.jtag_write([0x03], [0x00]) # Idle
        self.jtag_command(0x3a)
        self.jtag_command(0x02)
        return flen
    
    # Send bytes over SPI
    def fpga_spi(self, bytestring):
        buf=[]
        for h in bytestring.split(): buf.append(int(h, 16))
        self.spi_write(buf)

# Entry point
if __name__ == "__main__":
    # Parse CLI arguments
    parser=argparse.ArgumentParser(description="USB JTAG programmer for Gowin GW1NZ")
    parser.add_argument("-s", action="store", dest="s_filename", help="Program SRAM")
    parser.add_argument("-f", action="store", dest="f_filename", help="Program FLASH")
    parser.add_argument("-t", action="store", dest="spi", help="Transmit SPI bytes")
    parser.add_argument("-vget", action="store_true", default=False, dest="vget", help="Get USB bus voltage")
    parser.add_argument("-pget", action="store_true", default=False, dest="pget", help="Get current PMU control byte")
    parser.add_argument("-pset", action="store", dest="pset", help="Set current PMU control byte")
    parser.add_argument("-prget", action="store_true", default=False, dest="prget", help="Get persistent PMU control byte")
    parser.add_argument("-prset", action="store", dest="prset", help="Set persistent PMU control byte")
    parser.add_argument("-cget", action="store_true", default=False, dest="cget", help="Get current CLKOUT control byte")
    parser.add_argument("-cset", action="store", dest="cset", help="Set current CLKOUT control byte")
    parser.add_argument("-crget", action="store_true", default=False, dest="crget", help="Get persistent CLKOUT control byte")
    parser.add_argument("-crset", action="store", dest="crset", help="Set persistent CLKOUT control byte")
    parser.add_argument("-sget", action="store_true", default=False, dest="sget", help="Get serial number")
    parser.add_argument("-sset", action="store", dest="sset", help="Set serial number")
    parser.add_argument("-frst", action="store_true", default=False, dest="frst", help="Reset FPGA")
    parser.add_argument("-misp", action="store_true", default=False, dest="misp", help="Enter MCU ISP mode")
    args=parser.parse_args()
    
    jtag=GowinJTAG()

    # MCU ISP mode
    if args.misp:
        jtag.mcu_isp()
        time.sleep(1)
        sys.exit(0)
    
    # Read VBUS
    if args.vget:
        ret=jtag.read_vbus()
        if not ret: print("Error: failed to read VBUS voltage.")
        else: print("USB bus voltage is: {:.2f}V.".format(ret))
    
    # Read control bytes
    if args.pget:
        ret=jtag.read_control(0)
        if not ret: print("Error: failed to read configuration byte.")
        else: print("Current PMU configuration byte: {:02x}.".format(ret[0]))
    
    if args.prget:
        ret=jtag.read_control(1)
        if not ret: print("Error: failed to read configuration byte.")
        else: print("Persistent PMU configuration byte: {:02x}.".format(ret[0]))
    
    if args.cget:
        ret=jtag.read_control(2)
        if not ret: print("Error: failed to read configuration byte.")
        else: print("Current CLKOUT configuration byte: {:02x}.".format(ret[0]))
    
    if args.crget:
        ret=jtag.read_control(3)
        if not ret: print("Error: failed to read configuration byte.")
        else: print("Persistent CLKOUT configuration byte: {:02x}.".format(ret[0]))
    
    # Write control bytes
    if args.pset:
        jtag.write_control(0, args.pset)
    
    if args.prset:
        jtag.write_control(1, args.pset)
    
    if args.cset:
        jtag.write_control(2, args.pset)
    
    if args.crset:
        jtag.write_control(3, args.pset)
    
    # Get serial number
    if args.sget:
        ret=jtag.read_sn()
        if not ret: print("Error: failed to read serial number.")
        else: print("Serial number is "+ret+".")

    # Set serial number
    if args.sset:
        if len(args.sset)!=16:
            print("Error: serial number must be 16 characters long.")
            del jtag
            sys.exit(-1)
        jtag.write_sn(args.sset)

    # Reset FPGA
    if args.frst:
        jtag.fpga_rst()
    
    # Program SRAM
    if args.s_filename:
        # Check for input bitstream file existence
        if not os.path.exists(args.s_filename):
            print("Error: file does not exist.")
            del jtag
            sys.exit(-1)
        
        # Reset FPGA
        jtag.fpga_rst()
        jtag.jtag_rst()
        jtag.fpga_clear()

        # Read JTAG ID
        id=jtag.fpga_id()
        if not id:
            print("Error: failed to read device ID.")
            del jtag
            sys.exit(-1)
        print("Device ID is 0x"+id+".")

        # Program SRAM
        tbegin=time.time()
        flen=jtag.fpga_sram(args.s_filename)
        tend=time.time()
        print("{0:.2f} KiB programmed to SRAM in {1:.2f} ms at {2:.2f} Mbps.".format(flen/1024, (tend-tbegin)*1000, flen/(tend-tbegin)/125000))
    
    # SPI transfer
    if args.spi:
        jtag.fpga_spi(args.spi)
    
    del jtag
    sys.exit(0)