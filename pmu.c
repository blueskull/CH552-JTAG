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

// PMU control byte, default 0x6f
// Bit 0: FPGA enable, 0->disabled, 1->enabled
// Bit 1: FPGA VIO voltage sel, 0->1.8V, 1->3.3V
// Bit 2: 5V IO0 enable, 0->disabled, 1->enabled
// Bit 3: 5V IO0 direction mode, 0->internal, 1->external
// Bit 4: 5V IO0 internal direction, 0->input, 1->output
// Bit 5: 5V IO1 enable, 0->disabled, 1->enabled
// Bit 6: 5V IO1 direction mode, 0->internal, 1->external
// Bit 7: 5V IO1 internal direction, 0->input, 1->output

// CLKOUT control byte, default 0x03
// Bit 0: clock enable, 0->disabled, 1->enabled
// Bit 1: idle level, 0->low, 1->high
// Bit 2~7: clock divider value

// Control bytes
uint8_t byte_pmu=0x6f;
uint8_t byte_clk=0x03;

// Read PMU control byte
uint8_t pmu_read(void)
{
	return byte_pmu;
}

// Write PMU control byte
void pmu_write(uint8_t val)
{
	uint8_t i;
	// Start condition
	SDA=0; udelay(5); SCL=0;
	// Shift chip address
	for(i=8;i>0;) {SDA=0x10&(1<<--i); udelay(5); SCL=1; udelay(5); SCL=0;}
	SDA=1; udelay(5); SCL=1; udelay(5); SCL=0;
	// Shift register address
	for(i=8;i>0;) {SDA=0xf4&(1<<--i); udelay(5); SCL=1; udelay(5); SCL=0;}
	SDA=1; udelay(5); SCL=1; udelay(5); SCL=0;
	// Shift data
	for(i=8;i>0;) {SDA=val&(1<<--i); udelay(5); SCL=1; udelay(5); SCL=0;}
	SDA=1; udelay(5); SCL=1; udelay(5); SCL=0;
	// Stop condition
	SDA=0; udelay(5); SCL=1; udelay(5); SDA=1;
	byte_pmu=val;
}

// Read CLKOUT control byte
uint8_t clk_read(void)
{
	return byte_clk;
}

// Write CLKOUT control byte
void clk_write(uint8_t val)
{
	if(val&0x01)
	{
		RCAP2=0xffff-(val>>2);
		T2MOD=0xc2; // Fast mode, connect output
		T2CON=0x04; // Enable timer
	}
	else
	{
		T2CON=0x00; // Disable timer
		T2MOD=0x00; // Disconnect output
		CLK=val&0x02?1:0;
	}
	byte_clk=val;
}

// Initialize PMU
void pmu_init(void)
{
	uint8_t rom;
	// Read PMU configuration byte
	rom_read(0x00, &rom, 1);
	pmu_write(rom);
	// Read CLKOUT configuration byte
	rom_read(0x01, &rom, 1);
	clk_write(rom);
}