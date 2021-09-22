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
#define usb_txdesc(desc) {len=sizeof(desc); len=len>*slen?*slen:len; for(i=0;i<len;i++) buf_ep0[i]=desc[i];}

// USB descriptors
__code uint8_t desc_dev[]={0x12, 0x01, 0x00, 0x02, 0xff, 0xff, 0xff, 0x40, // USB2.0, vendor device, 64 bytes
						   0xa0, 0x20, 0x09, 0x42, 0x00, 0x01, // VID: 0x20a0, PID: 0x4209, V1.0
						   0x01, 0x02, 0x03, 0x01}; // Strings
__code uint8_t desc_cfg[]={0x09, 0x02, 0x20, 0x00, 0x01, 0x01, 0x00, 0x80, 0x64, // Bus powered, 100mA
						   0x09, 0x04, 0x00, 0x00, 0x02, 0xff, 0xff, 0xff, 0x00, // 2 endpoints, vendor interface
						   0x07, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00, // EP1 OUT, bulk, 64 bytes
						   0x07, 0x05, 0x82, 0x02, 0x40, 0x00, 0x00}; // EP2 IN, bulk, 64 bytes
__code uint8_t desc_lng[]={0x04, 0x03, 0x09, 0x04}; // en-US
__code uint8_t desc_ven[]={0x12, 0x03, 'B', 0, 'o', 0, '\'', 0, 's', 0, ' ', 0, 'L', 0, 'a', 0, 'b', 0};
__code uint8_t desc_pro[]={0x12, 0x03, 'U', 0, 'S', 0, 'B', 0, '-', 0, 'J', 0, 'T', 0, 'A', 0, 'G', 0};
__xdata uint8_t desc_ser[34]={0x22, 0x03}; // To be populated on power up

// Endpoint buffers
__xdata __at (0x0000) uint8_t buf_ep0[64];
__xdata __at (0x0040) uint8_t buf_ep1[2][64];
__xdata __at (0x00c0) uint8_t buf_ep2[64];
uint8_t len_ep1[2];
uint8_t idx_r_ep1;
#define buf_ep1_r buf_ep1[idx_r_ep1]
#define buf_ep1_w buf_ep1[1-idx_r_ep1]
#define len_ep1_r len_ep1[idx_r_ep1]
#define len_ep1_w len_ep1[1-idx_r_ep1]

// Initialize EP0
void ep0_init(void)
{
	UEP0_CTRL=0x02; // OUT ACK, IN NAK
	UEP0_DMA=(uint16_t)buf_ep0;
}

// Initialize EP1
void ep1_init(void)
{
	UEP4_1_MOD=0x80; // EP1 OUT, single buffer
	UEP1_CTRL=0x13; // OUT ACK, IN STALL, auto toggle
	idx_r_ep1=1; // Write to buffer 0 first
	UEP1_DMA=(uint16_t)buf_ep1_w;
	len_ep1_r=len_ep1_w=0;
}

// Initialize EP2
void ep2_init(void)
{
	UEP2_3_MOD=0x04; // EP2 IN, single buffer
	UEP2_CTRL=0x1e; // OUT STALL, IN NAK, auto toggle
	UEP2_DMA=(uint16_t)buf_ep2;
	UEP2_T_LEN=0;
}

// Swap EP1 buffer
void ep1_swap(void)
{
	idx_r_ep1=1-idx_r_ep1;
	UEP1_DMA=(uint16_t)buf_ep1_w;
}

// Get read buffer and call parser
void usb_rx(void(*parse)(uint8_t *buf, uint8_t len))
{
	uint8_t *buf, *len;
	while(!len_ep1_r);
	EA=0;
	buf=buf_ep1_r;
	len=&len_ep1_r;
	EA=1;
	parse(buf, *len);
	*len=0; // Invalidate buffer
	UEP1_CTRL&=~0x08; // ACK next
}

// Get EP2 write buffer
uint8_t *usb_buf(void)
{
	while(UEP2_T_LEN); // Wait until buffer is transmitted
	return buf_ep2;
}

// Commit EP2 write buffer
void usb_tx(uint8_t len)
{
	UEP2_T_LEN=len;
	UEP2_CTRL&=~0x02; // ACK next
}

// Initialize USB
void usb_init(void)
{
	uint8_t i;
	// Populate serial number descriptor
	rom_read(0x02, desc_ser+2, 16);
	for(i=16;i>0;) {i--; desc_ser[2+(i<<1)]=desc_ser[2+i]; desc_ser[3+(i<<1)]=0;}
	// Configure USB module
	IE_USB=0;
	USB_CTRL=0x00; // Deassert reset
	ep0_init();
	ep1_init();
	ep2_init();
	USB_DEV_AD=0x00;
	UDEV_CTRL=0x08; // Disable USB port
	USB_CTRL=0x29; // Enable, auto NAK, DMA
	UDEV_CTRL|=0x01; // Enable USB port
	USB_INT_FG=0xff; // Clear all
	USB_INT_EN=0x03; // Transfer and reset
	IE_USB=1;
}

// Finished receiving SETUP from EP0
void usb_setup(uint8_t *scode, uint8_t *slen, uint8_t *config)
{
	uint8_t len, i;
	UEP0_CTRL&=~0x01; // NAK next
	if(USB_RX_LEN!=8) goto ep0_stall;
	*slen=buf_ep0[6];
	if(buf_ep0[7]) *slen=64;
	len=0;
	*scode=buf_ep0[1];
	if((buf_ep0[0]&0x60)==0) // Standard request
		if(*scode==0x06) // Get descriptor
		{
			if(buf_ep0[3]==0x01) usb_txdesc(desc_dev) // Device descriptor
			else if(buf_ep0[3]==0x02) usb_txdesc(desc_cfg) // Config descriptor
			else if(buf_ep0[3]==0x03) // String descriptors
			{
				if(buf_ep0[2]==0x00) usb_txdesc(desc_lng)
				else if(buf_ep0[2]==0x01) usb_txdesc(desc_ven)
				else if(buf_ep0[2]==0x02) usb_txdesc(desc_pro)
				else if(buf_ep0[2]==0x03) usb_txdesc(desc_ser)
				else goto ep0_stall;
			}
			else goto ep0_stall;
		}
		else if(*scode==0x05) *slen=buf_ep0[2]; // Set address
		else if(*scode==0x08) {buf_ep0[0]=*config; len=*slen?1:0;} // Get config
		else if(*scode==0x09) *config=buf_ep0[2]; // Set config
		else if(*scode==0x0a) {buf_ep0[0]=0x00; len=*slen?1:0;} // Get interface
		else if(*scode==0x00) // Get status
		{
			// Is EP1 OUT stall
			if(buf_ep0[0]==0x82 && buf_ep0[4]==0x01) buf_ep0[0]=(UEP1_CTRL&0x0c)==0x0c?1:0;
			// Is EP2 IN stall
			else if(buf_ep0[0]==0x82 && buf_ep0[4]==0x82) buf_ep0[0]=(UEP2_CTRL&0x03)==0x03?1:0;
			else buf_ep0[0]=0;
			buf_ep0[1]=0;
			len=*slen>2?2:*slen;
		}
		else if(buf_ep0[0]==0x02 && *scode==0x03) // Set feature
			// Set EP1 OUT stall
			if(buf_ep0[4]==0x01) UEP1_CTRL|=0x0c;
			// Set EP2 IN stall
			else if(buf_ep0[4]==0x82) UEP2_CTRL|=0x03;
			else goto ep0_stall;
		else if(buf_ep0[0]==0x02 && *scode==0x01) // Clear feature
			// Clear EP1 OUT stall and toggle
			if(buf_ep0[4]==0x01) UEP1_CTRL=UEP1_CTRL&~0x8c;
			// Clear EP2 IN stall and toggle
			else if(buf_ep0[4]==0x82) UEP2_CTRL=UEP2_CTRL&~0x43;
			else goto ep0_stall;
		else goto ep0_stall;
	else
	{
ep0_stall:
		*slen=*scode=0x00;
		UEP0_CTRL=0x03; // IN STALL, DATA1
		return;
	}
	UEP0_T_LEN=len; // Max 64 bytes
	UEP0_CTRL=0xc0; // DATA1
}

// USB interrupt handler
void usb_int(void)
{
	static uint8_t scode=0, slen=0, config=0;
	uint8_t token;
	if(UIF_TRANSFER) // Transfer done
	{
		token=USB_INT_ST&0x3f;
		if(token==0x01 && U_TOG_OK) // EP1 OUT
		{
			if(len_ep1_r) UEP1_CTRL|=0x08; // Not processed yet, NAK next
			len_ep1_w=USB_RX_LEN;
			ep1_swap();
		}
		else if(token==0x22 && U_TOG_OK) // EP2 IN
		{
			UEP2_T_LEN=0;
			UEP2_CTRL|=0x02; // NAK next
		}
		else if(token==0x30) // EP0 SETUP
			usb_setup(&scode, &slen, &config);
		else if(token==0x20) // EP0 IN
		{
			if(scode==0x05) USB_DEV_AD=slen;
			UEP0_T_LEN=0; // Send ZLP
			UEP0_CTRL=0x02; // IN NAK
		}
		else if(token==0x00) // EP0 OUT
			UEP0_CTRL=0x02; // IN NAK
		UIF_TRANSFER=0;
	}
	else if(UIF_BUS_RST) // Bus reset
	{
		ep0_init();
		ep1_init();
		ep2_init();
		USB_DEV_AD=0x00;
		scode=slen=config=0;
		USB_INT_FG=0xff;
	}
	else USB_INT_FG=0xff;
}