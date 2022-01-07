#!/usr/bin/env node

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

const util=require("util");
const exec=util.promisify(require("child_process").exec);
const UsbJtag=require("./usbjtag");
const jtag=new UsbJtag;

async function cliOpen() {
	try {await jtag.open();} catch(e) {console.log(e); process.exit(-1);}
	process.on("SIGINT", ()=> {jtag.close();});
}

function cliClose(ret) {jtag.close(); process.exit(ret);}

function cliHelp() {
	const name=__filename.slice(__dirname.length+1);
	console.log("USBJTAG for Gowin GW1NZ FPGAs\nUsage:");
	console.log(`    node ${name} read <ctl|ctl_rom|sn|vbus>`);
	console.log(`    node ${name} write <ctl|ctl_rom|sn|spi|sram|flash|mcu> <value|bytes*|file>`);
	console.log("    *: SPI bytes are space separated and SPI transactions are 's' splitted.")
	console.log("Copyright (c) 2020-2021, Bo Gao, licensed under BSD 3-clause license.");
}

async function cliRead(dest) {
	try {
		if(dest=="ctl") {await cliOpen(); console.log("CTL: "+("0"+(await jtag.mcuReadReg(0)).toString(16)).slice(-2)); cliClose(0);}
		else if(dest=="ctl_rom") {await cliOpen(); console.log("CTL_ROM: "+("0"+(await jtag.mcuReadReg(1)).toString(16)).slice(-2)); cliClose(0);}
		else if(dest=="vbus") {await cliOpen(); console.log("VBUS: "+(await jtag.mcuReadVbus()).toFixed(2)+"V"); cliClose(0);}
		else if(dest=="sn") {await cliOpen(); console.log("SN: "+await jtag.mcuReadSn()); cliClose(0);}
		else cliHelp();
	}
	catch(e) {console.log(e); cliClose(-1);}
}

async function cliWrite(dest, val) {
	try {
		if(dest=="ctl") {await cliOpen(); await jtag.mcuWriteReg(parseInt(val, 16), 0); cliClose(0);}
		else if(dest=="ctl_rom") {await cliOpen(); await jtag.mcuWriteReg(parseInt(val, 16), 1); cliClose(0);}
		else if(dest=="sn") {await cliOpen(); await jtag.mcuWriteSn(val); cliClose(0);}
		else if(dest=="sram") {
			await cliOpen();
			await jtag.fpgaRstPwr();
			await jtag.fpgaRstTap(); // Reset FPGA
			await jtag.fpgaRstSram();
			console.log("JTAG ID: "+await jtag.fpgaReadId()); // Read ID
			const tStart=process.hrtime(); // Start timing
			const len=await jtag.fpgaWriteSram(val); // Program SRAM
			const tProg=process.hrtime(tStart);
			const tProgMs=tProg[0]*1000+tProg[1]/1000000;
			const cfgDone=await jtag.fpgaReadCdn(); // Check CDONE status
			console.log(`CDONE status: ${cfgDone?"success":"fail"}\nFile size: ${(len/1024).toFixed(2)} KiB\nElapsed time: ${tProgMs.toFixed(2)} ms\nAverage bitrate: ${(len/125/tProgMs).toFixed(2)} Mbps`);
			if(!cfgDone) throw("Error: SRAM bitstream corrupted.");
			cliClose(0);
		}
		else if(dest=="flash") {
			await cliOpen();
			await jtag.fpgaRstCfg();
			await jtag.fpgaRstTap(); // Reset FPGA
			await jtag.fpgaRstSram();
			console.log("JTAG ID: "+await jtag.fpgaReadId()); // Read ID
			const tStart=process.hrtime(); // Start timing
			const len=await jtag.fpgaWriteFlash(val); // Program flash
			const tProg=process.hrtime(tStart);
			const tProgMs=tProg[0]*1000+tProg[1]/1000000;
			const cfgDone=await jtag.fpgaReadCdn(); // Check CDONE status
			console.log(`CDONE status: ${cfgDone?"success":"fail"}\nFile size: ${(len/1024).toFixed(2)} KiB\nElapsed time: ${tProgMs.toFixed(2)} ms\nAverage bitrate: ${(len/125/tProgMs).toFixed(2)} Mbps`);
			if(!cfgDone) throw("Error: Flash bitstream corrupted.");
			cliClose(0);
		}
		else if(dest=="mcu") {
			await cliOpen();
			const ctl=await jtag.mcuReadReg(1);
			const sn=await jtag.mcuReadSn();
			await jtag.mcuRstIsp();
			jtag.close();
			await util.promisify((t, f)=>setTimeout(f, t))(1000);
			console.log((await exec(`python -m ch55xtool -r -f ${val}`)).stdout);
			await util.promisify((t, f)=>setTimeout(f, t))(1000);
			try {await jtag.open();} catch(e) {console.log(e); process.exit(-1);}
			await jtag.mcuWriteReg(ctl, 1);
			await jtag.mcuWriteSn(sn);
			cliClose(0);
		}
	}
	catch(e) {console.log(e); cliClose(-1);}
}

async function cliWriteSpi(val) {
	let opened=false;
	try {
		let tx=[];
		let rx=[];
		let txStr=[];
		let rxStr=[];
		let grpCount=0;
		const hexRe=/^[0-9a-fA-F]{1,2}$/;
		for(let i=0; i<val.length;i++) {
			if(hexRe.test(val[i])) tx.push(parseInt(val[i], 16)); // Hex check
			else if(val[i]!="s") throw("Error: invalid byte inputs");
			if((i==val.length-1 || val[i]=="s") && tx.length) { // Transmit
				if(!opened) {await cliOpen(); opened=true;}
				txStr[grpCount]="";
				for(let j=0; j<tx.length;j++) txStr[grpCount]+=("0"+tx[j].toString(16)).slice(-2)+(j==tx.length-1?"":" ");
				rx=await jtag.fpgaCallSpi(tx);
				rxStr[grpCount]="";
				for(let j=0; j<rx.length;j++) rxStr[grpCount]+=("0"+rx[j].toString(16)).slice(-2)+(j==rx.length-1?"":" ");
				tx=[];
				grpCount++;
			}
		}
		for(let i=0; i<txStr.length;i++) console.log("Transmitted bytes: "+txStr[i]);
		for(let i=0; i<rxStr.length;i++) console.log("Received    bytes: "+rxStr[i]);
		cliClose(0);
	}
	catch(e) {console.log(e); if(opened) cliClose(-1);}
}

async function main() {
	const argv=process.argv;
	const argc=argv.length;
	if(argc<=3) {cliHelp(); process.exit(-1);} // Invalid
	if(argc==4) {if(argv[2]=="read") await cliRead(argv[3]); else {cliHelp(); process.exit(-1);}} // Read register
	if(argc>=5) {
		if(argc==5 && argv[2]=="write" && argv[3]!="spi") await cliWrite(argv[3], argv[4]); // Write register or file
		else if(argv[2]=="write" && argv[3]=="spi") await cliWriteSpi(argv.slice(4, argv.length), false); // Write SPI
		else {cliHelp(); process.exit(-1);}
	}
}

main();