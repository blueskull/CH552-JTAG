/*
CH552 USB-JTAG Adapter for PicoGate

Copyright (c) 2020-2021, Bo Gao <7zlaser@gmail.com>

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of
   conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list
   of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors may be
   used to endorse or promote products derived from this software without specific
   prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THE
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "usbjtag.h"

// Return a byte to USB
void usb_ret(uint8_t val)
{
	*usb_buf()=val;
	usb_tx(1);
}

// Read control bytes
uint8_t ctl_read(uint8_t idx)
{
	uint8_t val;
	if(idx==0) usb_ret(pmu_read());
	else if(idx==1) {rom_read(0x00, &val, 1); usb_ret(val);}
	else if(idx==2) usb_ret(clk_read());
	else if(idx==3) {rom_read(0x01, &val, 1); usb_ret(val);}
	else return 1;
	return 0;
}

// Write control bytes
uint8_t ctl_write(uint8_t idx, uint8_t val)
{
	if(idx==0) pmu_write(val);
	else if(idx==1) rom_write(0x00, &val, 1);
	else if(idx==2) clk_write(val);
	else if(idx==3) rom_write(0x01, &val, 1);
	else return 1;
	return 0;
}

// USB packet processing
void usb_parse(uint8_t __xdata *buf, uint8_t len)
{
	uint8_t __xdata *cmd=buf;
	uint8_t __xdata *arg=buf+1;
	uint8_t __xdata *dat=buf+2;
	static uint8_t err=0;
	if(len<2) return; // Invalid packet
	len-=2;
	if(*cmd==0x00) // JTAG adapter control
	{
		// [0x00, 0x00]
		if(*arg==0x00) {if(len==0) {RST=0; udelay(20); RST=1; udelay(80); err=0;} else err=1;} // Reset FPGA
		// [0x00, 0x01, index]
		else if(*arg==0x01) {if(len==1) {err=ctl_read(*dat);} else err=1;} // Read control bytes
		// [0x00, 0x02, index, value]
		else if(*arg==0x02) {if(len==2) {err=ctl_write(*dat, *(dat+1));} else err=1;} // Write control bytes
		// [0x00, 0x03]
		else if(*arg==0x03) {if(len==0) {ADC_CTRL|=0x10; while(ADC_CTRL&0x10); usb_ret(ADC_DATA); err=0;} else err=1;} // Read ADC value
		// [0x00, 0x04, ms, us]
		else if(*arg==0x04) {if(len==2) {if(*dat) mdelay(*dat); if(*(dat+1)) udelay(*(dat+1)); err=0;} else err=1;} // Delay
		// [0x00, 0x05]
		else if(*arg==0x05) {if(len==0) {usb_ret(err); err=0;} else err=1;} // Get error status
		// [0x00, 0xfd]
		else if(*arg==0xfd) {if(len==0) {rom_read(0x02, usb_buf(), 16); usb_tx(16); err=0;} else err=1;} // Write serial number
		// [0x00, 0xfe, byte x16]
		else if(*arg==0xfe) {if(len==16) {rom_write(0x02, dat, 16); err=0;} else err=1;} // Write serial number
		// [0x00, 0xff]
		else if(*arg==0xff) {if(len==0) isp_jump(); else err=1;} // Enter ISP mode
		else err=1;
	}
	else if(*cmd==0x01 && len) // JTAG operation, must have a data payload
	{
		if(*arg==0x00) {if(!(len&1)) {jtag_write(dat, dat+(len>>1), len>>1); err=0;} else err=1;} // JTAG write
		else if(*arg==0x01) {if(!(len&1)) {jtag_write_read(dat, dat+(len>>1), usb_buf(), len>>1); usb_tx(len>>1); err=0;} else err=1;} // JTAG write and read
		else if(*arg==0x02) {spi_write(dat, len, 1); err=0;}
		else if(*arg==0x03) {spi_write_read(dat, usb_buf(), len, 1); usb_tx(len); err=0;}
		else err=1;
	}
	else if(*cmd==0x02 && len) // SPI operation, must have a data payload
	{
		if(*arg==0x00) {spi_write(dat, len, 0); err=0;}
		else if(*arg==0x01) {spi_write_read(dat, usb_buf(), len, 0); usb_tx(len); err=0;}
		else err=1;
	}
	else err=1;
}

// USB interrupt handler
void usb_isr(void) __interrupt(INT_NO_USB) __using(1)
{
	usb_int();
}

// Entry point
void main(void)
{
	clk_init();
	pin_init();
	adc_init();
	pmu_init();
	spi_init(2);
	usb_init();
	udelay(100);
	RST=1;
	EA=1;
	while(1) // Main USB packet loop
		usb_rx(usb_parse);
}