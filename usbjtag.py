#!/usr/bin/env python3

# Copyright (c) 2020, Bo Gao <7zlaser@gmail.com>

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

import sys, usb, binascii

class GowinJTAG:
    dev=None

    # Open USB device
    def __init__(self):
        self.dev=usb.core.find(idVendor=0x20a0, idProduct=0x4209)
        if self.dev is None:
            print("Error: device not found")
            sys.exit(-1)
        self.dev.set_configuration()

    # Write JTAG command
    def WriteCmd(self, cmd):
        self.dev.write(0x01, [0x10, 0x30, 0x80, 0x01, 0x00, cmd&0xff, 0x00])

    # Reset FPGA
    def ResetPwr(self):
        self.dev.write(0x01, [0x00])
        time.sleep(0.01)
        self.dev.write(0x01, [0x01])

    # Reset JTAG
    def ResetTap(self):
        self.dev.write(0x01, [0x10, 0xff, 0x00, 0x00, 0x00])

    # Reset SRAM
    def ResetRam(self):
        self.WriteCmd(0x15) # Enter config mode
        self.WriteCmd(0x05) # Clear SRAM
        self.WriteCmd(0x02) # Nop
        time.sleep(0.01) # Wait for finish
        self.WriteCmd(0x3a) # Exit config mode
        self.WriteCmd(0x02) # Nop

    # Read JTAG ID
    def ReadId(self):
        self.WriteCmd(0x11) # Read JTAG ID command
        self.dev.write(0x01, [0x11, 0x20, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
        ret=self.dev.read(0x81, 64, 100)
        ret.reverse()
        return bytes(ret[1:5]).hex()

    # Write SRAM
    def WriteRam(self, filename):
        self.WriteCmd(0x15) # Enter config mode
        self.WriteCmd(0x17) # Write SRAM
        self.dev.write(0x01, [0x10, 0x20, 0x00]) # Shift DR
        with open(filename, "rb") as f:
            while True:
                dat=f.read(63)
                if not dat:
                    break
                # SPI mode, MSB first
                self.dev.write(0x01, [0x20]+list(dat))
        self.dev.write(0x01, [0x10, 0x03, 0x00]) # Idle
        self.WriteCmd(0x3a) # Exit config mode
        self.WriteCmd(0x02) # Nop

    # Enter ISP mode
    def EnterIsp(self):
        try: # Connection WILL break
            self.dev.write(0x01, [0xff])
        except Exception: pass

# Entry point
if __name__ == "__main__":
    import argparse
    import time
    import os
    parser=argparse.ArgumentParser(description="USB JTAG programmer for Gowin FPGAs")
    parser.add_argument("-r", action="store_true", default=False, dest="reset_fpga", help="Reset FPGA")
    parser.add_argument("-c", action="store_true", default=False, dest="clear_sram", help="Clear SRAM content")
    parser.add_argument("-s", action="store_true", default=False, dest="prog_sram", help="Program SRAM")
    parser.add_argument("-f", action="store", dest="prog_filename", help="Specify SRAM filename")
    parser.add_argument("-i", action="store_true", default=False, dest="isp_mode", help="Enter adapter ISP mode")
    parser.add_argument("-v", action="store_true", default=False, dest="verbose_mode", help="Verbose output")
    args=parser.parse_args()
    jtag=GowinJTAG()
    if args.isp_mode:
        jtag.EnterIsp()
        sys.exit(0)
    if args.reset_fpga:
        jtag.ResetPwr()
    jtag.ResetTap()
    if args.verbose_mode:
        print("Device ID is 0x"+jtag.ReadId())
    if args.clear_sram or args.prog_sram:
        jtag.ResetRam()
    if args.prog_sram:
        if args.prog_filename is None:
            print("Error: filename not specified")
            sys.exit(-1)
        if not os.path.exists(args.prog_filename):
            print("Error: cannot open file")
            sys.exit(-1)
        tbegin=time.time()
        jtag.WriteRam(args.prog_filename)
        tend=time.time()
        if args.verbose_mode:
            print("Programming done in {0:.2f}s.".format(tend-tbegin))
    sys.exit(0)
