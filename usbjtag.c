/*
CH552 USB-JTAG Adapter

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

//Pin map
#define TMS P14
#define TCK P17
#define TDI P15
#define TDO P16
#define ENA P33
#define RST P32
#define ISP P10 //Not used, pull down

//Helper macros
#define jtag_bit(mask) {TMS=tms&mask; TDI=tdi&mask; SAFE_MOD++; TCK=1; TCK=0;}
#define jtag_bit_read(mask) {TMS=tms&mask; TDI=tdi&mask; SAFE_MOD++; tdo|=TDO?mask:0; TCK=1; TCK=0;}
#define usb_txdesc(desc) {len=sizeof(desc); len=len>*slen?*slen:len; for(i=0;i<len;i++) buf_ep0[i]=desc[i];}

//USB descriptors
__code static uint8_t desc_dev[]={0x12, 0x01, 0x10, 0x01, 0xff, 0xff, 0xff, 0x40, 0xa0, 0x20, 0x09, 0x42, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01};
__code static uint8_t desc_cfg[]={0x09, 0x02, 0x20, 0x00, 0x01, 0x01, 0x00, 0x80, 0xfa, 0x09, 0x04, 0x00, 0x00, 0x02, 0xff, 0xff, 0xff, 0x00, 0x07, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00, 0x07, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00};
__code static uint8_t desc_lng[]={0x04, 0x03, 0x09, 0x04};
__code static uint8_t desc_pro[]={0x12, 0x03, 'U', 0, 'S', 0, 'B', 0, '-', 0, 'J', 0, 'T', 0, 'A', 0, 'G', 0};

//Endpoint buffers
__xdata __at (0x0000) uint8_t buf_ep0[64];
__xdata __at (0x0040) uint8_t buf_ep1[128];

//Entry point
void main(void)
{
	SAFE_MOD=0x55;
	SAFE_MOD=0xAA;
	CLOCK_CFG=CLOCK_CFG&~0x07|0x05; //16MHz
	SAFE_MOD=0x00;
	P1&=~(0x80|0x20|0x10|0x91); //TCK, TDI, TMS, ISP low
	P1_DIR_PU&=~(0x40); //TDO IN
	P1_MOD_OC&=~(0x80|0x40|0x20|0x10|0x01); //TCK, TDI, TMS, ISP PP, TDI IN
	P3&=~(0x08|0x04); //ENA, RST low
	P3_MOD_OC&=~(0x08|0x04); //RST, ENA PP
	IE_USB=0;
	USB_CTRL=0x00; //Deassert reset
	UEP4_1_MOD=0xc0; //EP1 IN/OUT
	UEP0_DMA=(uint16_t)buf_ep0;
	UEP1_DMA=(uint16_t)buf_ep1;
	UEP0_CTRL=0x02; //OUT ACK, IN NAK
	UEP1_CTRL=0x12; //OUT ACK, IN NAK
	USB_DEV_AD=0x00;
	UDEV_CTRL=0x08; //Disable USB port
	USB_CTRL=0x29; //ENA, auto NAK, DMA
	UDEV_CTRL|=0x01; //Enable USB port
	USB_INT_FG=0xff; //Clear all
	USB_INT_EN=0x03; //XFER, RST
	IE_USB=1;
	SPI0_CK_SE=0x02; //SCK 8MHz
	EA=1;
	while(1);
}

//JTAG write N bytes
void jtag_write(uint8_t *mbuf, uint8_t *obuf, uint8_t N)
{
	uint8_t i, tms, tdi;
	for(i=0;i<N;i++)
	{
		tms=mbuf[i]; //XRAM access is slow
		tdi=obuf[i];
		jtag_bit(0x01) jtag_bit(0x02) jtag_bit(0x04) jtag_bit(0x08)
		jtag_bit(0x10) jtag_bit(0x20) jtag_bit(0x40) jtag_bit(0x80)
	}
}

//JTAG write and read N bytes
void jtag_write_read(uint8_t *mbuf, uint8_t *obuf, uint8_t *ibuf, uint8_t N)
{
	uint8_t i, tms, tdi, tdo;
	for(i=0;i<N;i++)
	{
		tms=mbuf[i]; //XRAM access is slow
		tdi=obuf[i];
		tdo=0;
		jtag_bit_read(0x01) jtag_bit_read(0x02) jtag_bit_read(0x04) jtag_bit_read(0x08)
		jtag_bit_read(0x10) jtag_bit_read(0x20) jtag_bit_read(0x40) jtag_bit_read(0x80)
		ibuf[i]=tdo;
	}
}

//SPI write N bytes
void spi_write(uint8_t *obuf, uint8_t N, uint8_t csena)
{
	uint8_t i;
	SPI0_CTRL=0x60;
	if(csena) TMS=0;
	for(i=0;i<N;i++)
	{
		SPI0_DATA=obuf[i];
		while(!S0_FREE);
	}
	if(csena) TMS=1;
	SPI0_CTRL=0x02;
}

//SPI write and read N bytes
void spi_write_read(uint8_t *obuf, uint8_t *ibuf, uint8_t N, uint8_t csena)
{
	uint8_t i;
	SPI0_CTRL=0x60;
	if(csena) TMS=0;
	for(i=0;i<N;i++)
	{
		SPI0_DATA=obuf[i];
		while(!S0_FREE);
		ibuf[i]=SPI0_DATA;
	}
	if(csena) TMS=1;
	SPI0_CTRL=0x02;
}

//Finished receiving SETUP from EP0
inline void ep0_setup(uint8_t *scode, uint8_t *slen, uint8_t *config)
{
	uint8_t len, i;
	UEP0_CTRL&=~0x01; //STALL->NAK
	if(USB_RX_LEN!=8) goto ep0_stall;
	*slen=buf_ep0[6];
	if(buf_ep0[7]) *slen=64;
	len=0;
	*scode=buf_ep0[1];
	if((buf_ep0[0]&0x60)==0) //Standard request
		if(*scode==0x06) //Get descriptor
		{
			if(buf_ep0[3]==0x01) usb_txdesc(desc_dev) //Device
			else if(buf_ep0[3]==0x02) usb_txdesc(desc_cfg) //Config
			else if(buf_ep0[3]==0x03) //String
			{
				if(buf_ep0[2]==0x00) usb_txdesc(desc_lng)
				else if(buf_ep0[2]==0x01) usb_txdesc(desc_pro)
				else goto ep0_stall;
			}
			else goto ep0_stall;
		}
		else if(*scode==0x05) *slen=buf_ep0[2]; //Set address
		else if(*scode==0x08) {buf_ep0[0]=*config; len=*slen?1:0;} //Get config
		else if(*scode==0x09) *config=buf_ep0[2]; //Set config
		else if(*scode==0x0a) {buf_ep0[0]=0x00; len=*slen?1:0;} //Get interface
		else if(*scode==0x00) //Get status
		{
			if(buf_ep0[0]==0x82 && buf_ep0[4]==0x81) buf_ep0[0]=(UEP1_CTRL&0x03)==0x03?1:0;
			else if(buf_ep0[0]==0x82 && buf_ep0[4]==0x01) buf_ep0[0]=(UEP1_CTRL&0x0c)==0x0c?1:0;
			else buf_ep0[0]=0;
			buf_ep0[1]=0;
			len=*slen>2?2:*slen;
		}
		else if(buf_ep0[0]==0x02 && *scode==0x03) //Set feature
			if(buf_ep0[4]==0x81) UEP1_CTRL|=0x03;
			else if(buf_ep0[4]==0x01) UEP1_CTRL|=0x0c;
			else goto ep0_stall;
		else if(buf_ep0[0]==0x02 && *scode==0x01) //Clear feature
			if(buf_ep0[4]==0x81) UEP1_CTRL=(UEP1_CTRL&~0x43)|0x02;
			else if(buf_ep0[4]==0x01) UEP1_CTRL=(UEP1_CTRL&~0x8c);
			else goto ep0_stall;
		else goto ep0_stall;
	else
	{
ep0_stall:
		*slen=*scode=0x00;
		UEP0_CTRL=0x03; //IN STALL, DATA1
		return;
	}
	UEP0_T_LEN=len; //Max 64 bytes
	UEP0_CTRL=0xc0; //DATA1
}

//Finished receiving from EP1
inline void ep1_out(void)
{
	uint8_t len=USB_RX_LEN;
	uint8_t cmd=buf_ep1[0];
	if(!U_TOG_OK) return;
	if(len<1) return;
	if((cmd&0xf0)==0x00) {RST=cmd&0x01; ENA=cmd&0x02;} //Power control
	else if((cmd&0xf0)==0x10 && len&1) //JTAG write
		if(!(cmd&0x01)) jtag_write(&buf_ep1[1], &buf_ep1[1+len>>1], len>>1); //Write only
		else //Write and read
		{
			jtag_write_read(&buf_ep1[1], &buf_ep1[1+len>>1], &buf_ep1[64], len>>1);
			UEP1_T_LEN=len>>1;
			UEP1_CTRL&=~0x02;
		}
	else if((cmd&0xf0)==0x20) //SPI write
		if(!(cmd&0x01)) spi_write(&buf_ep1[1], len-1, cmd&0x02); //Write only
		else //Write and read
		{
			spi_write_read(&buf_ep1[1], &buf_ep1[64], len-1, cmd&0x02);
			UEP1_T_LEN=len-1;
			UEP1_CTRL&=~0x02;
		}
	else if(cmd==0xff) //Enter ISP mode
	{
		EA=0;
		USB_CTRL=0x06; //Reset USB
		USB_INT_FG=0xff;
		for(len=0;len<255;len++) //Delay
			for(cmd=0;cmd<255;cmd++) {SAFE_MOD++; SAFE_MOD++; SAFE_MOD++; SAFE_MOD++;}
		((void (*)(void))0x3800)(); //Jump to bootloader
	}
}

//USB interrupt handler
void usb_isr(void) __interrupt (INT_NO_USB) __using 1
{
	static uint8_t scode=0, slen=0, config=0;
	uint8_t token;
	if(UIF_TRANSFER) //Transfer done
	{
		token=USB_INT_ST&0x3f;
		if(token==0x21) //EP1 IN
			UEP1_CTRL|=0x02; //EP1 IN NAK
		else if(token==0x01) //EP1 OUT
			ep1_out();
		else if(token==0x30) //EP0 SETUP
			ep0_setup(&scode, &slen, &config);
		else if(token==0x20) //EP0 IN
		{
			if(scode==0x05) USB_DEV_AD=slen;
			UEP0_T_LEN=0; //Send ZLP
			UEP0_CTRL=0x02; //IN NAK
		}
		else if(token==0x00) //EP0 OUT
			UEP0_CTRL=0x02; //IN NAK
		UIF_TRANSFER=0;
	}
	else if(UIF_BUS_RST) //Bus reset
	{
		UEP0_CTRL=0x02;
		UEP1_CTRL=0x12;
		USB_DEV_AD=0x00;
		scode=slen=config=0;
		USB_INT_FG=0xff;
	}
	else USB_INT_FG=0xff;
}
