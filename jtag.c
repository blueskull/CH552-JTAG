/*
CH552 USB-JTAG Adapter JTAG Logic File

Copyright (c) 2020, Bo Gao <7zlaser@gmail.com>

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

#include <compiler.h>
#include <stdint.h>
#include "ch552.h"

//JTAG pin map
#define TMS P14
#define TCK P17
#define TDO P15
#define TDI P16

//SPI chip select map
#define NCS P14

//Power control pin map
#define ENA P33
#define RST P32
#define ISP P10

//JTAG status flag
uint8_t status;

//Initialize JTAG and SPI
void jtag_init()
{
	P1&=~0x91; //TMS, TCK, ISP low
	P1_DIR_PU&=~0x40; //TDI IN
	P1_MOD_OC&=~0xf1; //TMS, TCK, TDO, ISP PP, TDI IN
	P3&=~0x0c; //ENA, RST low
	P3_MOD_OC&=~0x0c; //RST, ENA PP
	SPI0_CK_SE=0x02; //8MHz
}

//JTAG, SPI and power control
//Flags bit 0: DCDC enable
//Flags bit 1: FPGA enable
//Flags bit 2: UART mode
void jtag_power(uint8_t flags)
{
	ENA=flags&0x01?1:0;
	RST=flags&0x02?1:0;
	SPI0_CTRL=flags&0x04?0x60:0x02;
	status=flags;
}

//Send n JTAG pulses and read response
inline uint8_t jtag_pulse(uint8_t tms, uint8_t tdo, uint8_t n)
{
	uint8_t ret=0;
	uint8_t i;
	uint8_t mask=1;
	for(i=0;i<n;i++)
	{
		TMS=tms&mask; //LSB first
		TDO=tdo&mask; //LSB first
		if(TDI) ret|=mask;
		mask<<=1;
		TCK=1;
		TCK=0;
	}
	return ret;
}

//Write N JTAG bytes and n JTAG bits
void jtag_write(uint8_t *tms, uint8_t *tdo, uint8_t *tdi, uint8_t N, uint8_t n)
{
	uint8_t i;
	for(i=0;i<N;i++)
		tdi[i]=jtag_pulse(tms[i], tdo[i], 8);
	if(n) tdi[i]=jtag_pulse(tms[i], tdo[i], n);
}

//SPI write array and read last response
uint8_t spi_write(uint8_t *s, uint8_t len, uint8_t cs)
{
	uint8_t i;
	uint8_t r=0;
	if(cs) NCS=0;
	for(i=0;i<len;i++)
	{
		SPI0_DATA=s[i];
		while(!S0_FREE);
	}
	r=SPI0_DATA;
	if(cs) NCS=1;
	return r;
}
