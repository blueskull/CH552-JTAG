/*
CH552 USB-JTAG Adapter USB Device Driver File

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
#include <stdlib.h>
#include "ch552.h"
#include "desc.h"

void jtag_power(uint8_t flags);
void jtag_write(uint8_t *tms, uint8_t *tdo, uint8_t *tdi, uint8_t N, uint8_t n);
void spi_write(uint8_t *s, int len);

//Delay n microseconds
void udelay(uint8_t n)
{
	while(n--) {SAFE_MOD++; SAFE_MOD++; SAFE_MOD++;}
}

//Delay n milliseconds
void mdelay(uint16_t n)
{
	while(n--)
	{
		udelay(250);
		udelay(250);
		udelay(250);
		udelay(250);
	}
}

//Enter ISP mode
static void (* __data isp_jump)(void)=(void(*)(void))0x3800;

//Endpoint buffers
__xdata __at (0x0000) uint8_t buf_ep0[64];
__xdata __at (0x0040) uint8_t buf_ep1[128];

//Internal USB state variables
static uint8_t scode, slen, config;

//JTAG status flag
extern uint8_t status;

//Initialize USB
void usb_init(void)
{
	IE_USB=0;
	USB_CTRL=0x00; //Deassert reset
	UEP4_1_MOD=0xc0; //EP1 IN/OUT
	UEP0_DMA=(uint16_t)buf_ep0;
	UEP1_DMA=(uint16_t)buf_ep1;
	UEP0_CTRL=0x02; //OUT ACK, IN NAK
	UEP1_CTRL=0x12; //OUT ACK, IN ACK
	USB_DEV_AD=0x00;
	UDEV_CTRL=0x08; //Disable port
	USB_CTRL=0x29; //ENA, auto NAK, DMA
	UDEV_CTRL|=0x01; //Enable port
	USB_INT_FG=0xff; //Clear all
	USB_INT_EN=0x03; //XFER, RST
	IE_USB=1;
}

//Buffer copy, identical to memcpy
static inline void bufcpy(uint8_t* d, const uint8_t* s, uint8_t l)
{
	while(l--) *d++=*s++;
}

//Prepare a dewcriptor for uploading
#define txdesc(desc) {len=sizeof(desc);len=len>slen?slen:len;bufcpy(buf_ep0, desc, len);}

//Finished receiving SETUP from EP0
static inline void ep0_setup(void)
{
	uint8_t len;
	UEP0_CTRL&=~0x01; //STALL->NAK
	if(USB_RX_LEN!=8) goto ep0_stall;
	slen=buf_ep0[6];
	if(buf_ep0[7]) slen=64;
	len=0;
	scode=buf_ep0[1];
	if((buf_ep0[0]&0x60)==0) //Standard request
		if(scode==0x06) //Get descriptor
		{
			if(buf_ep0[3]==0x01) //Device
				txdesc(desc_dev)
			else if(buf_ep0[3]==0x02) //Config
				txdesc(desc_cfg)
			else if(buf_ep0[3]==0x03) //String
			{
				if(buf_ep0[2]==0x00) //Language
					txdesc(desc_lng)
				else if(buf_ep0[2]==0x01) //Vendor
					txdesc(desc_ven)
				else if(buf_ep0[2]==0x02) //Product
					txdesc(desc_pro)
				else if(buf_ep0[2]==0xee) //OSSD
					txdesc(desc_osd)
				else goto ep0_stall;
			}
			else goto ep0_stall;
		}
		else if(scode==0x05) //Set address
			slen=buf_ep0[2];
		else if(scode==0x08) //Get configuration
		{
			buf_ep0[0]=config;
			len=slen?1:0;
		}
		else if(scode==0x09) //Set configuration
			config=buf_ep0[2];
		else if(scode==0x0a) //Get interface
		{
			buf_ep0[0]=0x00;
			len=slen?1:0;
		}
		else if(scode==0x00) //Get status
		{
			if(buf_ep0[0]==0x82 && buf_ep0[4]==0x81)
				buf_ep0[0]=(UEP1_CTRL&0x03)==0x03?1:0;
			else if(buf_ep0[0]==0x82 && buf_ep0[4]==0x01)
				buf_ep0[0]=(UEP1_CTRL&0x0c)==0x0c?1:0;
			else buf_ep0[0]=0;
			buf_ep0[1]=0;
			len=slen>2?2:slen;
		}
		else if(buf_ep0[0]==0x02 && scode==0x03) //Set feature
			if(buf_ep0[4]==0x81) //EP1 IN
				UEP1_CTRL|=0x03;
			else if(buf_ep0[4]==0x01) //EP1 OUT
				UEP1_CTRL|=0x0c;
			else goto ep0_stall;
		else if(buf_ep0[0]==0x02 && scode==0x01) //Clear feature
			if(buf_ep0[4]==0x81) //EP1 IN
				UEP1_CTRL=(UEP1_CTRL&~0x43)|0x02; //IN NAK
			else if(buf_ep0[4]==0x01) //EP1 OUT
				UEP1_CTRL=(UEP1_CTRL&~0x8c); //OUT ACK
			else goto ep0_stall;
		else goto ep0_stall;
	else if(buf_ep0[0]==0xc0 && buf_ep0[4]==0x04) //WCID
		txdesc(desc_cid)
	else
	{
ep0_stall:
		slen=scode=0x00;
		UEP0_CTRL=0x03; //IN STALL, DATA1
		return;
	}
	UEP0_T_LEN=len; //Max 64 bytes
	UEP0_CTRL=0xc0; //DATA1
}

//Finished receiving from EP1
static inline void ep1_out(void)
{
	uint8_t len=USB_RX_LEN;
	uint8_t cmd=buf_ep1[0];
	uint8_t val1=buf_ep1[1];
	uint8_t val2=buf_ep1[2];
	if(!U_TOG_OK) return;
	if(len<1) return;
	if(cmd==0x00) //Power control
		jtag_power(val1);
	else if(cmd==0x01 && !(status&0x04)) //JTAG write-then-read
		if(val1>30 || (val1>29 && val2) || (2*(val1+(val2?1:0))+4!=len))
			UEP1_CTRL|=0x02; //Length check
		else
		{
			jtag_write(&buf_ep1[4], &buf_ep1[4+val1+(val2?1:0)], &buf_ep1[64], val1, val2);
			UEP1_T_LEN=val1+(val2?1:0);
			UEP1_CTRL&=~0x02;
		}
	else if(cmd==0x02 && (status&0x04)) //SPI write
		spi_write(&buf_ep1[1], len-1);
	else if(cmd==0xff) //Enter ISP mode
	{
		EA=0;
		USB_CTRL=0x06;
		USB_INT_FG=0xff;
		mdelay(500);
		isp_jump();
	}
}

void usb_isr(void)
{
	uint8_t token;
	if(UIF_TRANSFER) //Transfer done
	{
		token=USB_INT_ST&0x3f;
		if(token==0x21) //EP1 IN
			UEP1_CTRL|=0x02; //EP1 IN NAK
		else if(token==0x01) //EP1 OUT
			ep1_out();
		else if(token==0x30) //EP0 SETUP
			ep0_setup();
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
