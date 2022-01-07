// Copyright (c) 2020-2021, Bo Gao <7zlaser@gmail.com>

// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:

// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THE
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

const usb=require("usb");
const util=require('util');
const fs=require("fs");

class UsbJtag {
	// Private members
	#rx;
	#tx;

	// Find and open device, initialize endpoints
	async open() {
		// Find device
		this.dev=usb.findByIds(0x20a0, 0x4209);
		if(!this.dev) throw("Error: cannot find USB device.");
		// Open and configure device
		try {
			this.dev.open(true);
			this.dev.interfaces[0].claim();
			const rxe=this.dev.interfaces[0].endpoint(0x82);
			const txe=this.dev.interfaces[0].endpoint(0x01);
			rxe.timeout=100;
			txe.timeout=100;
			// Promisify callback functions
			this.rx=util.promisify(rxe.transfer).bind(rxe);
			this.tx=util.promisify(txe.transfer).bind(txe);
		}
		catch(e) {throw("Error: failed to open USB device.");}
		// Flush input buffer
		try {await this.rx(64);} catch(e) {;}
	}

	// Close device
	close() {this.dev.close();}

	// Read bytes from device
	async usbRead() {return await this.rx(64);}

	// Write bytes to device
	async usbWrite(cmd, arg, dat=[]) {return await this.tx([cmd, arg, ...dat]);}

	// Write bytes to then read bytes from device
	async usbCall(cmd, arg, dat=[], len=-1) {
		await this.usbWrite(cmd, arg, dat);
		const ret=await this.usbRead();
		if(len>=0 && ret.length!=len) throw("Error: invalid response length.");
		return ret;
	}

	// Hard reset FPGA, opcode 0x00, 0x00, 0x00
	async fpgaRstPwr() {await this.usbWrite(0x00, 0x00, [0x00]);}

	// User reset FPGA, opcode 0x00, 0x00, 0x01
	async fpgaRstUsr() {await this.usbWrite(0x00, 0x00, [0x01]);}

	// Read control byte, opcode 0x00, 0x01
	async mcuReadReg(rom) {
		if(rom) return (await this.usbCall(0x00, 0x01, [0x01], 1))[0];
		return (await this.usbCall(0x00, 0x01, [0x00], 1))[0];
	}

	// Write control bytes, opcode 0x00, 0x02
	async mcuWriteReg(val, rom) {
		if(rom) await this.usbWrite(0x00, 0x02, [0x01, val]);
		await this.usbWrite(0x00, 0x02, [0x00, val]);
	}

	// Read USB bus voltage, opcode 0x00, 0x03
	async mcuReadVbus() {return (await this.usbCall(0x00, 0x03, [], 1))[0]*0.0258;}

	// Delay ms plus us, opcode 0x00, 0x04, 0x00
	async mcuDelay(ms, us) {
		if(ms<0 || ms>255 || us<0 || us>255) throw("Error: value out of bound.");
		await this.usbWrite(0x00, 0x04, [0x00, ms, us]);
	}

	// Read error status, opcode 0x00, 0x05
	async mcuReadErr() {return (await this.usbCall(0x00, 0x05, [], 1))[0];}

	// Read serial number, opcode 0x00, 0xfd
	async mcuReadSn() {return String.fromCharCode.apply(null, await this.usbCall(0x00, 0xfd, [], 16)).replace(/\0/g, "");}

	// Write serial number, opcode 0x00, 0xfe
	async mcuWriteSn(val) {
		if(val.length<1 || val.length>16) throw("Error: invalid input length.");
		await this.usbWrite(0x00, 0xfe, val.split("").map((c)=> {return c.charCodeAt()}));
	}

	// Reset into ISP mode, opcode 0x00, 0xff
	async mcuRstIsp() {try {await this.usbWrite(0x00, 0xff);} catch(e) {;}}

	// JTAG write, opcode 0x01, 0x00/0x02
	async fpgaWriteJtag(tms, tdi, fast=false) {
		if(fast) {
			if(tdi.length<1 || tdi.length>62) throw("Error: invalid input length.");
			await this.usbWrite(0x01, 0x02, tdi);
		}
		else {
			if(tdi.length<1 || tdi.length>31 || tdi.length!=tms.length) throw("Error: invalid input length.");
			await this.usbWrite(0x01, 0x00, [...tms, ...tdi]);
		}
	}

	// JTAG write and read, opcode 0x01, 0x01/0x03
	async fpgaCallJtag(tms, tdi, fast=false) {
		if(fast) {
			if(tdi.length<1 || tdi.length>62) throw("Error: invalid input length.");
			return await this.usbCall(0x01, 0x03, tdi, tdi.length);
		}
		else {
			if(tdi.length<1 || tdi.length>31 || tdi.length!=tms.length) throw("Error: invalid input length.");
			return await this.usbCall(0x01, 0x01, [...tms, ...tdi], tdi.length);
		}
	}

	// JTAG pulse TCK, opcode 0x00, 0x04, 0x01
	async fpgaPulseJtag(ms, us) {
		if(ms<0 || ms>255 || us<0 || us>255) throw("Error: value out of bound.");
		await this.usbWrite(0x00, 0x04, [0x01, ms, us]);
	}

	// SPI write, opcode 0x02, 0x00
	async fpgaWriteSpi(dat) {
		if(dat.length<1 || dat.length>62) throw("Error: invalid input length.");
		await this.usbWrite(0x02, 0x00, dat);
	}

	// SPI write and read, opcode 0x02, 0x01
	async fpgaCallSpi(dat) {
		if(dat.length<1 || dat.length>62) throw("Error: invalid input length.");
		return await this.usbCall(0x02, 0x01, dat, dat.length);
	}

	// Reset TAP
	async fpgaRstTap() {await this.fpgaWriteJtag([0xff], [0x00]);}

	// Send JTAG command
	async fpgaWriteCmd(cmd) {await this.fpgaWriteJtag([0x30, 0x80, 0x01], [0x00, cmd&0xff, 0x00]);}

	// Read JTAG register
	async fpgaReadReg(reg) {
		await this.fpgaWriteCmd(reg);
		return (await this.fpgaCallJtag([0x20, 0x00, 0x00, 0x00, 0x80, 0x01], [0x00, 0x00, 0x00, 0x00, 0x00, 0x00])).reverse().slice(1, 5);
	}

	// Read FPGA ID
	async fpgaReadId() {return Array.from(await this.fpgaReadReg(0x11), (b)=> {return ("0"+b.toString(16)).slice(-2);}).join("");}

	// Read FPGA CDONE status
	async fpgaReadCdn() {return ((await this.fpgaReadReg(0x41))[2]&0x20)?true:false;}

	// FPGA reset SRAM
	async fpgaRstSram() {
		await this.fpgaWriteCmd(0x15);
		await this.fpgaWriteCmd(0x05);
		await this.fpgaWriteCmd(0x02);
		await this.mcuDelay(10, 0);
		await this.fpgaWriteCmd(0x09);
		await this.fpgaWriteCmd(0x3a);
		await this.fpgaWriteCmd(0x02);
	}

	// FPGA program SRAM
	async fpgaWriteSram(filename) {
		await this.fpgaWriteCmd(0x15);
		await this.fpgaWriteCmd(0x12);
		await this.fpgaWriteCmd(0x17);
		await this.fpgaWriteJtag([0x20], [0x00]); // Shift-DR
		const buf=await fs.promises.readFile(filename);
		if(!buf) throw("Error: cannot read input file.");
		const len=buf.length;
		for(let i=0;i<buf.length;i+=62) {
			const chunk=buf.slice(i, (len-i)>62?62+i:len);
			await this.fpgaWriteJtag([], [...chunk], true);
		}
		await this.fpgaWriteJtag([0x03], [0x00]); // Idle
		await this.fpgaWriteCmd(0x3a);
		await this.fpgaWriteCmd(0x02);
		return len;
	}

	// FPGA program flash
	async fpgaWriteFlash(filename) {
		throw("Error: function not implemented.");
	}
}

module.exports=UsbJtag;