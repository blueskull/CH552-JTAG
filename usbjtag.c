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

#include <compiler.h>
#include <stdint.h>
#include "ch552.h"

// MCU pin map
#define VIS P11 // VBUS SNS/CH552 ADC CH0
#define TDI P15 // GW1NZ TDI/CH552 MOSI
#define TDO P16 // GW1NZ TDO/CH552 MISO
#define TCK P17 // GW1NZ TCK/CH552 SCK
#define TMS P30 // GW1NZ TMS/CH552 NCS_N
#define JEN P31 // GW1NZ JTAGEN_N
#define URS P32 // GW1NZ USRRST_N
#define SDA P33 // SLG46580 I2C SDA
#define SCL P34 // SLG46580 I2C SCL
// #define SCL P15
// #define SDA P16

// Helper macros
#define pin_mode(p, n, m, v) {p##_MOD_OC=(m==0 || m==1)?p##_MOD_OC&~(1<<n):p##_MOD_OC|(1<<n); p##_DIR_PU=(m==0 || m==2)?p##_DIR_PU&~(1<<n):p##_DIR_PU|(1<<n); p##n=v;}
#define pmu_byte (((ctl_byte&0x01)?0x03:0x00)|((ctl_byte&0x02)?0x04:0x00))
#define nop __asm__("nop");
#define nops {nop nop nop nop nop nop nop nop nop nop nop nop nop nop}
#define spi_tx {__asm__("movx a, @dptr"); __asm__("mov _SPI0_DATA, a"); nops}
#define spi_tx8 {spi_tx spi_tx spi_tx spi_tx spi_tx spi_tx spi_tx spi_tx}
#define spi_rx {XBUS_AUX=0x04; spi_tx XBUS_AUX=0x05; __asm__("mov a, _SPI0_DATA"); __asm__("movx @dptr, a");}
#define spi_rx8 {spi_rx spi_rx spi_rx spi_rx spi_rx spi_rx spi_rx spi_rx}
#define jtag_txb(m) {TMS=tms&m; TDI=tdi&m; SAFE_MOD++; TCK=1; SAFE_MOD++; TCK=0;}
#define jtag_tx {jtag_txb(0x01) jtag_txb(0x02) jtag_txb(0x04) jtag_txb(0x08) jtag_txb(0x10) jtag_txb(0x20) jtag_txb(0x40) jtag_txb(0x80)}
#define jtag_rxb(m) {TMS=tms&m; TDI=tdi&m; SAFE_MOD++; tdo|=TDO?m:0; TCK=1; SAFE_MOD++; TCK=0;}
#define jtag_rx {jtag_rxb(0x01) jtag_rxb(0x02) jtag_rxb(0x04) jtag_rxb(0x08) jtag_rxb(0x10) jtag_rxb(0x20) jtag_rxb(0x40) jtag_rxb(0x80)}
#define usb_txdesc(d) {len=sizeof(d); len=len>*slen?*slen:len; for(i=0;i<len;i++) buf_ep0[i]=d[i];}
#define usb_txstr(d) {usb_txdesc(d); if(buf_ep0[0]>len) buf_ep0[0]=len;}
#define usb_tx(l) {UEP2_T_LEN=l; UEP2_CTRL&=~0x02;}
#define usb_ret(v) {while(UEP2_T_LEN); buf_ep2[0]=v; usb_tx(1)}
#define i2c_tx(b) {for(i=8;i>0;) {SDA=b&(1<<--i); udelay(5); SCL=1; udelay(5); SCL=0;} SDA=1; udelay(5); SCL=1; udelay(5); SCL=0;}

// USB descriptors
__code uint8_t desc_dev[]={0x12, 0x01, 0x00, 0x02, 0xff, 0xff, 0xff, 0x40, // USB2.0, vendor device, 64 bytes
						   0xa0, 0x20, 0x09, 0x42, 0x00, 0x01, // VID: 0x20a0, PID: 0x4209, V1.0
						   0x01, 0x02, 0x00, 0x01}; // Strings
__code uint8_t desc_cfg[]={0x09, 0x02, 0x20, 0x00, 0x01, 0x01, 0x00, 0x80, 0x64, // Bus powered, 100mA
						   0x09, 0x04, 0x00, 0x00, 0x02, 0xff, 0xff, 0xff, 0x00, // 2 endpoints, vendor interface
						   0x07, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00, // EP1 OUT, bulk, 64 bytes
						   0x07, 0x05, 0x82, 0x02, 0x40, 0x00, 0x00}; // EP2 IN, bulk, 64 bytes
__code uint8_t desc_lng[]={0x04, 0x03, 0x09, 0x04}; // en-US
__code uint8_t desc_ven[]={0x12, 0x03, 'B', 0, 'o', 0, '\'', 0, 's', 0, ' ', 0, 'L', 0, 'a', 0, 'b', 0};
__code uint8_t desc_pro[]={0x12, 0x03, 'U', 0, 'S', 0, 'B', 0, '-', 0, 'J', 0, 'T', 0, 'A', 0, 'G', 0};

// Endpoint buffers
__xdata __at (0x0000) uint8_t buf_ep0[64];
__xdata __at (0x0080) uint8_t buf_ep1[2][64];
__xdata __at (0x0040) uint8_t buf_ep2[64];
uint8_t len_ep1[2], idx_r_ep1;
#define buf_ep1_r buf_ep1[idx_r_ep1]
#define buf_ep1_w buf_ep1[1-idx_r_ep1]
#define len_ep1_r len_ep1[idx_r_ep1]
#define len_ep1_w len_ep1[1-idx_r_ep1]

// Delay functions
void udelay(uint8_t us) {while(us--) {SAFE_MOD++; SAFE_MOD++; SAFE_MOD++;}}
void mdelay(uint8_t ms) {while(ms--) {udelay(250); udelay(250); udelay(250); udelay(242);}}

// PMU and CLKOUT functions
void pmu_write(uint8_t val) {uint8_t i; SDA=0; udelay(5); SCL=0; i2c_tx(0x10) i2c_tx(0xf4) i2c_tx(val) SDA=0; udelay(5); SCL=1; udelay(5); SDA=1;}
void clk_on(uint8_t div, uint8_t jtag) {if(jtag) {PIN_FUNC&=~0x01; pin_mode(P1, 4, 3, 1) pin_mode(P1, 7, 0, 0) pin_mode(P1, 0, 1, 0)} else {PIN_FUNC|=0x01; pin_mode(P1, 0, 0, 0) pin_mode(P1, 7, 1, 0) pin_mode(P1, 4, 1, 0)} RCAP2=0xffff-div; T2MOD=0xc2; T2CON=0x04;}
void clk_off(void) {pin_mode(P1, 0, 0, 0) pin_mode(P1, 7, 1, 0) pin_mode(P1, 4, 3, 1) T2CON=0x00; T2MOD=0x00; RCAP2=0x00;}

// Write or read control byte
uint8_t ctl_write(uint8_t mask, uint8_t opr, uint8_t val)
{
	static uint8_t ctl_byte=0x00;
	if(mask==0x00 && opr==0x01 && ctl_byte&0x01) {URS=0; pmu_write(pmu_byte); udelay(250); pmu_write(pmu_byte|0x08); mdelay(40); URS=1; return 0;} // Reset FPGA
	if(mask==0x00 && opr==0x02 && ctl_byte&0x01) {URS=0; udelay(250); URS=1; return 0;} // Reset user logic
	if(mask==0x00 && opr==0x03 && ctl_byte&0x01) {if(val) clk_on(3, 1); else if(ctl_byte&0x04) clk_on((ctl_byte&0x08)?0:7, 0); else clk_off(); return 0;} // Pulse TCK RTI clock
	if(mask==0x00 && opr==0x04) {if(val) {ADC_CFG=0x08; pmu_write(pmu_byte|0x10);} else {ADC_CFG=0x00; pmu_write(pmu_byte&~0x10);} return 0;} // Enable Vbus divider
	if(mask==0x00) return ctl_byte; // No special operation and no bit mask defined
	if(ctl_byte&0x80) return ctl_byte; // CFG_LOCK
	uint8_t ctl_old=ctl_byte;
	ctl_byte=ctl_byte&(val|~mask)|(mask&val);
	if(ctl_byte&0x04 && ctl_byte&0x01) clk_on((ctl_byte&0x08)?0:7, 0); // Enable clock
	if(!(ctl_byte&0x04) || !(ctl_byte&0x01)) clk_off(); // Disable clock
	if((ctl_old^ctl_byte)&0x01) // Power status changed
	{
		URS=0; pmu_write(0x00); mdelay(10); pmu_write(0x08); URS=1; // Power status change, power off first
		if(ctl_byte&0x01) {URS=0; pmu_write(pmu_byte); mdelay(5); pmu_write(pmu_byte|0x08); mdelay(40); URS=1;} // Power on
	}
	return ctl_byte;
}

// Read data flash
void rom_read(uint8_t add, uint8_t *buf, uint8_t len)
{
	uint8_t i;
	ROM_ADDR_H=0xc0;
	for(i=0;i<len;i++) {ROM_ADDR_L=(add+i)<<1; ROM_CTRL=0x8e; buf[i]=ROM_DATA_L;}
}

// Write data flash
void rom_write(uint8_t add, uint8_t *buf, uint8_t len)
{
	uint8_t i;
	ROM_ADDR_H=0xc0; SAFE_MOD=0x55; SAFE_MOD=0xaa; GLOBAL_CFG|=0x04; SAFE_MOD=0x00; // Unlock flash
	for(i=0;i<len;i++) {ROM_ADDR_L=(add+i)<<1; ROM_DATA_L=buf[i]; ROM_CTRL=0x9a;}
	SAFE_MOD=0x55; SAFE_MOD=0xaa; GLOBAL_CFG&=~0x04; SAFE_MOD=0x00; // Lock flash
}

// SPI write bytes
void spi_write(uint8_t __xdata *obuf, uint8_t len, uint8_t jtag)
{
	SPI0_CTRL=0x60; // Enable SPI
	JEN=!jtag; if(!jtag) TMS=0; // Manipulate IOs
	XBUS_AUX=0x00; SAFE_MOD=obuf[0]; XBUS_AUX=0x04; // Populate DPTR 0
	while(len>=8) {spi_tx8 len-=8;} while(len--) {spi_tx} XBUS_AUX=0x00; // Transfer data
	if(!jtag) TMS=1;
	SPI0_CTRL=0x02; // Disable SPI
}

// SPI write and read bytes
void spi_write_read(uint8_t __xdata *obuf, uint8_t __xdata *ibuf, uint8_t len, uint8_t jtag)
{
	SPI0_CTRL=0x60; // Enable SPI
	JEN=!jtag; if(!jtag) TMS=0; // Manipulate IOs
	XBUS_AUX=0x00; SAFE_MOD=obuf[0]; XBUS_AUX=0x01; SAFE_MOD=ibuf[0]; XBUS_AUX=0x04; // Populate DPTRs
	while(len>=8) {spi_rx8 obuf+=8; len-=8;} while(len--) {spi_rx} XBUS_AUX=0x00; // Transfer data
	if(!jtag) TMS=1;
	SPI0_CTRL=0x02; // Disable SPI
}

// JTAG write bytes
void jtag_write(uint8_t __xdata *mbuf, uint8_t __xdata *obuf, uint8_t len)
{
	uint8_t i, tms, tdi;
	JEN=0;
	for(i=0;i<len;i++) {tms=mbuf[i]; tdi=obuf[i]; jtag_tx}
}

// JTAG write and read bytes
void jtag_write_read(uint8_t __xdata *mbuf, uint8_t __xdata *obuf, uint8_t __xdata *ibuf, uint8_t len)
{
	uint8_t i, tms, tdi, tdo;
	JEN=0;
	for(i=0;i<len;i++) {tms=mbuf[i]; tdi=obuf[i]; tdo=0; jtag_rx ibuf[i]=tdo;}
}

// USB packet processing
void usb_parse(uint8_t __xdata *buf, uint8_t len)
{
	uint8_t __xdata *cmd=buf;
	uint8_t __xdata *arg=buf+1;
	uint8_t __xdata *dat=buf+2;
	uint8_t val;
	static uint8_t err=0;
	if(len<2) return; // Invalid packet
	len-=2;
	if(*cmd==0x00) // JTAG adapter control
		if(*arg==0x00) {if(len==1) {if(*dat==0x00) {ctl_write(0x00, 0x01, 0x00); err=0;} else if(*dat==0x01) {ctl_write(0x00, 0x02, 0x00); err=0;} else err=1;} else err=1;} // [0x00, 0x00, user], reset FPGA
		else if(*arg==0x01) {if(len==1) {if(*dat==0x00) {usb_ret(ctl_write(0x00, 0x00, 0x00)) err=0;} else if(*dat==0x01) {rom_read(0x00, &val, 1); usb_ret(val) err=0;} else err=1;} else err=1;} // [0x00, 0x01, index], read control bytes
		else if(*arg==0x02) {if(len==2) {if(*dat==0) {ctl_write(0xff, 0x00, *(dat+1)); err=0;} else if(*dat==1) {rom_write(0x00, dat+1, 1); err=0;} else err=1;} else err=1;} // [0x00, 0x02, index, value], write control bytes
		else if(*arg==0x03) {if(len==0) {ctl_write(0x00, 0x04, 0x01); ADC_CTRL=0x10; while(ADC_CTRL&0x10); usb_ret(ADC_DATA) ctl_write(0x00, 0x04, 0x00); err=0;} else err=1;} // [0x00, 0x03], read ADC value
		else if(*arg==0x04) {if(len==3) {if(*dat==1) ctl_write(0x00, 0x03, 0x01); if(*(dat+1)) mdelay(*(dat+1)); if(*(dat+2)) udelay(*(dat+2)); if(*dat==1) ctl_write(0x00, 0x03, 0x00); err=0;} else err=1;} // [0x00, 0x04, pulse, ms, us], delay and pulse clock
		else if(*arg==0x05) {if(len==0) {usb_ret(err); err=0;} else err=1;} // [0x00, 0x05], get error status
		else if(*arg==0xfd) {if(len==0) {while(UEP2_T_LEN); rom_read(0x01, buf_ep2, 16); usb_tx(16); err=0;} else err=1;} // [0x00, 0xfd], read serial number
		else if(*arg==0xfe) {if(len<=16) {for(;len<16;len++) dat[len]=0x00; rom_write(0x01, dat, len); err=0;} else err=1;} // [0x00, 0xfe, bytes], Write serial number
		else if(*arg==0xff) {if(len==0) {EA=0; USB_CTRL=0x06; USB_INT_FG=0xff; mdelay(100); ((void (*)(void))0x3800)();} else err=1;} // [0x00, 0xff], enter ISP mode
		else err=1;
	else if(*cmd==0x01 && len) // JTAG operation, must have a data payload
		if(*arg==0x00) {if(!(len&1)) {jtag_write(dat, dat+(len>>1), len>>1); err=0;} else err=1;} // JTAG write
		else if(*arg==0x01) {if(!(len&1)) {while(UEP2_T_LEN); jtag_write_read(dat, dat+(len>>1), buf_ep2, len>>1); usb_tx(len>>1); err=0;} else err=1;} // JTAG write and read
		else if(*arg==0x02) {spi_write(dat, len, 1); err=0;}
		else if(*arg==0x03) {while(UEP2_T_LEN); spi_write_read(dat, buf_ep2, len, 1); usb_tx(len); err=0;}
		else err=1;
	else if(*cmd==0x02 && len) // SPI operation, must have a data payload
		if(*arg==0x00) {spi_write(dat, len, 0); err=0;}
		else if(*arg==0x01) {while(UEP2_T_LEN); spi_write_read(dat, buf_ep2, len, 0); usb_tx(len); err=0;}
		else err=1;
	else err=1;
}

// EP0 setup handler
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
				if(buf_ep0[2]==0x00) {usb_txdesc(desc_lng) buf_ep0[0]=len;}
				else if(buf_ep0[2]==0x01) usb_txstr(desc_ven)
				else if(buf_ep0[2]==0x02) usb_txstr(desc_pro)
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
			if(buf_ep0[0]==0x82 && buf_ep0[4]==0x01) buf_ep0[0]=(UEP1_CTRL&0x0c)==0x0c?1:0; // Get EP1 OUT stall
			else if(buf_ep0[0]==0x82 && buf_ep0[4]==0x82) buf_ep0[0]=(UEP2_CTRL&0x03)==0x03?1:0; // Get EP2 IN stall
			else buf_ep0[0]=0;
			buf_ep0[1]=0; len=*slen>2?2:*slen;
		}
		else if(buf_ep0[0]==0x02 && *scode==0x03) // Set feature
			if(buf_ep0[4]==0x01) UEP1_CTRL|=0x0c; // Set EP1 OUT stall
			else if(buf_ep0[4]==0x82) UEP2_CTRL|=0x03; // Set EP2 IN stall
			else goto ep0_stall;
		else if(buf_ep0[0]==0x02 && *scode==0x01) // Clear feature
			if(buf_ep0[4]==0x01) UEP1_CTRL=UEP1_CTRL&~0x8c; // Reset EP1 OUT stall
			else if(buf_ep0[4]==0x82) UEP2_CTRL=UEP2_CTRL&~0x43; // Reset EP2 IN stall
			else goto ep0_stall;
		else goto ep0_stall;
	else {ep0_stall: *slen=*scode=0x00; UEP0_CTRL=0x03; return;}
	UEP0_T_LEN=len; UEP0_CTRL=0xc0; // Transmit as DATA1
}

// USB interrupt handler
void usb_isr(void) __interrupt(INT_NO_USB) __using(1)
{
	static uint8_t scode=0, slen=0, config=0;
	uint8_t token;
	if(UIF_TRANSFER) // Transfer done
	{
		token=USB_INT_ST&0x3f;
		if(token==0x01 && U_TOG_OK) {if(len_ep1_r) UEP1_CTRL|=0x08; len_ep1_w=USB_RX_LEN; idx_r_ep1=1-idx_r_ep1; UEP1_DMA=(uint16_t)buf_ep1_w;} // EP1 OUT
		else if(token==0x22 && U_TOG_OK) {UEP2_T_LEN=0; UEP2_CTRL|=0x02;} // EP2 IN
		else if(token==0x30) usb_setup(&scode, &slen, &config); // EP0 setup
		else if(token==0x20) {if(scode==0x05) USB_DEV_AD=slen; UEP0_T_LEN=0; UEP0_CTRL=0x02;} // EP0 IN
		else if(token==0x00) UEP0_CTRL=0x02; // EP0 OUT
		UIF_TRANSFER=0;
	}
	else if(UIF_BUS_RST) // Bus reset
	{
		UEP0_CTRL=0x02; // Reset EP0 IN/OUT
		UEP1_CTRL=0x13; idx_r_ep1=1; UEP1_DMA=(uint16_t)buf_ep1_w; len_ep1_r=len_ep1_w=0; // Reset EP1 OUT
		UEP2_CTRL=0x1e; UEP2_DMA=(uint16_t)buf_ep2; UEP2_T_LEN=0; // Reset EP2 IN
		USB_DEV_AD=0x00; scode=slen=config=0; USB_INT_FG=0xff; // Reset address and states
	}
	else USB_INT_FG=0xff;
}

// Entry point
void main(void)
{
	uint8_t rom;
	uint8_t __xdata *buf;
	uint8_t *len;
	SAFE_MOD=0x55; SAFE_MOD=0xaa; CLOCK_CFG=CLOCK_CFG&~0x07|0x05; SAFE_MOD=0x00; // Initialize clock at 16 MHz
	pin_mode(P1, 0, 0, 0) pin_mode(P1, 1, 0, 0) pin_mode(P1, 4, 3, 1) pin_mode(P1, 5, 1, 0) pin_mode(P1, 6, 0, 0) pin_mode(P1, 7, 1, 0) // Initialize P1
	pin_mode(P3, 0, 1, 1) pin_mode(P3, 1, 1, 1) pin_mode(P3, 2, 1, 1) pin_mode(P3, 3, 3, 1) pin_mode(P3, 4, 1, 1) // Initialize P3
	//pin_mode(P1, 5, 1, 1) pin_mode(P1, 6, 3, 1)
	rom_read(0x00, &rom, 1); if(rom==0xff) {rom=0x0f; rom_write(0x00, &rom, 1);} ctl_write(0xff, 0x00, rom); // Populate control byte
	SPI0_CK_SE=2; // Initialize SPI at 8 MHz
	IE_USB=0; USB_CTRL=0x00; // Reset USB
	UEP0_CTRL=0x02; UEP0_DMA=(uint16_t)buf_ep0; // Configure EP0 IN/OUT
	UEP4_1_MOD=0x80; UEP1_CTRL=0x13; idx_r_ep1=1; UEP1_DMA=(uint16_t)buf_ep1_w; len_ep1_r=len_ep1_w=0; // Configure EP1 OUT
	UEP2_3_MOD=0x04; UEP2_CTRL=0x1e; UEP2_DMA=(uint16_t)buf_ep2; UEP2_T_LEN=0; // Configure EP2 IN
	USB_DEV_AD=0x00; UDEV_CTRL=0x08; USB_CTRL=0x29; UDEV_CTRL|=0x01; USB_INT_FG=0xff; USB_INT_EN=0x03; IE_USB=1; EA=1; // Initialize USB
	while(1) {while(!len_ep1_r); EA=0; buf=buf_ep1_r; len=&len_ep1_r; EA=1; usb_parse(buf, *len); *len=0; UEP1_CTRL&=~0x08;} // Poll data and ACK on next request
}