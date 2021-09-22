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

// Helper macro
#define io_mode(port, pin, init, oc, pu) {port##pin=init; port##_MOD_OC=port##_MOD_OC&~(1<<pin)|(oc?1<<pin:0); port##_DIR_PU=port##_DIR_PU&~(1<<pin)|(pu?1<<pin:0);}

// Initialize clock
void clk_init(void)
{
	// Set clock to 16MHz
	SAFE_MOD=0x55;
	SAFE_MOD=0xaa;
	CLOCK_CFG=CLOCK_CFG&~0x07|0x05;
	SAFE_MOD=0x00;
}

// Initialize GPIO
void pin_init(void)
{
	/*
	io_mode(P1, 0, 1, 0, 1) // CLK_IN, high, PP
	io_mode(P1, 1, 1, 0, 1) // JTAGSEL_N, high, PP
	io_mode(P1, 4, 1, 0, 0) // ADC, input
	io_mode(P1, 5, 0, 0, 1) // TDI, low, PP
	io_mode(P1, 6, 1, 0, 0) // TDO, input
	io_mode(P1, 7, 0, 0, 1) // TCK, low, PP
	io_mode(P3, 0, 1, 1, 1) // SDA, high, bi-di
	io_mode(P3, 1, 1, 0, 1) // TMS, high, PP
	io_mode(P3, 2, 0, 0, 1) // RECONFIG_N, low, PP
	io_mode(P3, 3, 1, 1, 1) // SCL, high, bi-di
	*/
	io_mode(P1, 4, 1, 0, 1) // TMS, high, PP
	io_mode(P1, 5, 0, 0, 1) // TDI, low, PP
	io_mode(P1, 6, 1, 0, 0) // TDO, input
	io_mode(P1, 7, 0, 0, 1) // TCK, low, PP
}

// Initialize ADC
void adc_init(void)
{
	ADC_CTRL=0x01; // Channel P1.4
	ADC_CFG=0x08; // Enable ADC, slow mode
}

// Read data flash
void rom_read(uint8_t add, uint8_t *buf, uint8_t len)
{
	uint8_t i;
	ROM_ADDR_H=0xc0;
	for(i=0;i<len;i++)
	{
		ROM_ADDR_L=(add+i)<<1;
		ROM_CTRL=0x8e;
		buf[i]=ROM_DATA_L;
	}
}

// Write data flash
void rom_write(uint8_t add, uint8_t *buf, uint8_t len)
{
	uint8_t i;
	ROM_ADDR_H=0xc0;
	// Enable data flash writing
	SAFE_MOD=0x55;
	SAFE_MOD=0xaa;
	GLOBAL_CFG|=0x04;
	SAFE_MOD=0x00;
	// Write to data flash
	for(i=0;i<len;i++)
	{
		ROM_ADDR_L=(add+i)<<1;
		ROM_DATA_L=buf[i];
		ROM_CTRL=0x9a;
	}
	// Disable data flash writing
	SAFE_MOD=0x55;
	SAFE_MOD=0xaa;
	GLOBAL_CFG&=~0x04;
	SAFE_MOD=0x00;
}

// Microsecond delay
void udelay(uint8_t us)
{
	while(us--)
	{
		SAFE_MOD++;
		SAFE_MOD++;
		SAFE_MOD++;
	}
}

// Millisecond delay
void mdelay(uint8_t ms)
{
	while(ms--)
	{
		udelay(250);
		udelay(250);
		udelay(250);
		udelay(242);
	}
}

// Enter ISP mode
void isp_jump(void)
{
	EA=0;
	USB_CTRL=0x06; // Reset USB
	USB_INT_FG=0xff;
	mdelay(100); // Give host time to reenumerate
	((void (*)(void))0x3800)(); // Jump to bootloader
}