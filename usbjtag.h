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

#define TEST

// MCU pin map
#ifndef TEST
#define TMS P31 // GW1NZ JTAG TMS/CH552 SPI NCS
#define TCK P17 // GW1NZ JTAG TCK/CH552 SPI SCK
#define TDI P15 // GW1NZ JTAG TDI/CH552 SPI SDO
#define TDO P16 // GW1NZ JTAG TDO/CH552 SPI SDI
#else
#define TMS P14
#define TCK P17
#define TDI P15
#define TDO P16
#endif
#define JEN P11 // GW1NZ JTAGSEL_N
#define RST P32 // GW1NZ RECONFIG_N
#define CLK P10 // GW1NZ CLK_IN
#define SCL P33 // SLG46580 I2C SCL
#define SDA P30 // SLG46580 I2C SDA

// Initialization routine prototypes
void clk_init(void);
void pin_init(void);
void adc_init(void);
void pmu_init(void);
void spi_init(uint8_t cfg);
void usb_init(void);

// Read and write routine prototypes
void rom_read(uint8_t add, uint8_t *buf, uint8_t len);
void rom_write(uint8_t add, uint8_t *buf, uint8_t len);
uint8_t pmu_read(void);
void pmu_write(uint8_t val);
uint8_t clk_read(void);
void clk_write(uint8_t val);
void spi_write(uint8_t __xdata *obuf, uint8_t len, uint8_t jtag);
void spi_write_read(uint8_t __xdata *obuf, uint8_t __xdata *ibuf, uint8_t len, uint8_t jtag);
void jtag_write(uint8_t __xdata *mbuf, uint8_t __xdata *obuf, uint8_t len);
void jtag_write_read(uint8_t __xdata *mbuf, uint8_t __xdata *obuf, uint8_t __xdata *ibuf, uint8_t len);
void usb_rx(void(*parse)(uint8_t __xdata *buf, uint8_t len));
uint8_t __xdata *usb_buf(void);
void usb_tx(uint8_t len);

// Miscellaneous routine prototypes
void mdelay(uint8_t ms);
void udelay(uint8_t us);
void isp_jump(void);
void usb_int(void);