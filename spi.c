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

// Helper macros
#define jtag_bit(mask) {TMS=tms&mask; TDI=tdi&mask; SAFE_MOD++; TCK=1; SAFE_MOD++; TCK=0;}
#define jtag_bit_read(mask) {TMS=tms&mask; TDI=tdi&mask; SAFE_MOD++; tdo|=TDO?mask:0; TCK=1; SAFE_MOD++; TCK=0;}

// Initialize SPI
void spi_init(uint8_t cfg)
{
	SPI0_CK_SE=cfg;
}

// SPI write N bytes
void spi_write(uint8_t *obuf, uint8_t len, uint8_t jtag)
{
	uint8_t i;
	SPI0_CTRL=0x60; // Enable SPI
	if(!jtag) {JEN=1; TMS=0;}
	while(len>=8)
	{
		SPI0_DATA=obuf[0];
		while(!S0_FREE);
		SPI0_DATA=obuf[1];
		while(!S0_FREE);
		SPI0_DATA=obuf[2];
		while(!S0_FREE);
		SPI0_DATA=obuf[3];
		while(!S0_FREE);
		SPI0_DATA=obuf[4];
		while(!S0_FREE);
		SPI0_DATA=obuf[5];
		while(!S0_FREE);
		SPI0_DATA=obuf[6];
		while(!S0_FREE);
		SPI0_DATA=obuf[7];
		while(!S0_FREE);
		len-=8;
		obuf+=8;
	}
	for(i=0;i<len;i++)
	{
		SPI0_DATA=obuf[i];
		while(!S0_FREE);
	}
	if(!jtag) TMS=1;
	SPI0_CTRL=0x02; // Disable SPI
}

// SPI write and read N bytes
void spi_write_read(uint8_t *obuf, uint8_t *ibuf, uint8_t len, uint8_t jtag)
{
	uint8_t i;
	SPI0_CTRL=0x60; // Enable SPI
	if(!jtag) {JEN=1; TMS=0;}
	while(len>=8)
	{
		SPI0_DATA=obuf[0];
		while(!S0_FREE);
		ibuf[0]=SPI0_DATA;
		SPI0_DATA=obuf[1];
		while(!S0_FREE);
		ibuf[1]=SPI0_DATA;
		SPI0_DATA=obuf[2];
		while(!S0_FREE);
		ibuf[2]=SPI0_DATA;
		SPI0_DATA=obuf[3];
		while(!S0_FREE);
		ibuf[3]=SPI0_DATA;
		SPI0_DATA=obuf[4];
		while(!S0_FREE);
		ibuf[4]=SPI0_DATA;
		SPI0_DATA=obuf[5];
		while(!S0_FREE);
		ibuf[5]=SPI0_DATA;
		SPI0_DATA=obuf[6];
		while(!S0_FREE);
		ibuf[6]=SPI0_DATA;
		SPI0_DATA=obuf[7];
		while(!S0_FREE);
		ibuf[7]=SPI0_DATA;
		len-=8;
		obuf+=8;
		ibuf+=8;
	}
	for(i=0;i<len;i++)
	{
		SPI0_DATA=obuf[i];
		while(!S0_FREE);
		ibuf[i]=SPI0_DATA;
	}
	if(!jtag) TMS=1;
	SPI0_CTRL=0x02; // Disable SPI
}

// JTAG write N bytes
void jtag_write(uint8_t *mbuf, uint8_t *obuf, uint8_t len)
{
	uint8_t i, tms, tdi;
	JEN=0;
	for(i=0;i<len;i++)
	{
		tms=mbuf[i]; // XRAM access is slow
		tdi=obuf[i];
		jtag_bit(0x01) jtag_bit(0x02) jtag_bit(0x04) jtag_bit(0x08)
		jtag_bit(0x10) jtag_bit(0x20) jtag_bit(0x40) jtag_bit(0x80)
	}
}

// JTAG write and read N bytes
void jtag_write_read(uint8_t *mbuf, uint8_t *obuf, uint8_t *ibuf, uint8_t len)
{
	uint8_t i, tms, tdi, tdo;
	JEN=0;
	for(i=0;i<len;i++)
	{
		tms=mbuf[i]; // XRAM access is slow
		tdi=obuf[i];
		tdo=0;
		jtag_bit_read(0x01) jtag_bit_read(0x02) jtag_bit_read(0x04) jtag_bit_read(0x08)
		jtag_bit_read(0x10) jtag_bit_read(0x20) jtag_bit_read(0x40) jtag_bit_read(0x80)
		ibuf[i]=tdo;
	}
}