/*
CH552 USB-JTAG Adapter USB Descriptor File

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

#include <stdint.h>

//Devbice descriptor
//USB 2.0, VID=0x861A, PID=0x2257, no serial number string
__code static uint8_t desc_dev[]=
{
	0x12, 0x01, 0x00, 0x02, 0xff, 0xff, 0xff, 0x40,
	0xa0, 0x20, 0x09, 0x42, 0x00, 0x01, 0x01, 0x02,
	0x00, 0x01
};

//Configuration and following descriptors
//Bus powered, 500mA, 1 interface, EP1 IN and EP1 OUT, bulk
__code static uint8_t desc_cfg[]=
{
	0x09, 0x02, 0x20, 0x00, 0x01, 0x01, 0x00, 0x80,
	0xfa, 0x09, 0x04, 0x00, 0x00, 0x02, 0xff, 0xff,
	0xff, 0x00, 0x07, 0x05, 0x81, 0x02, 0x40, 0x00,
	0x00, 0x07, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00
};

//String descriptors
__code static uint8_t desc_lng[]=
{
	0x04, 0x03, 0x09, 0x04
};

__code static uint8_t desc_ven[]=
{
	0x0a, 0x03, 'T', 0, 'e', 0, 's', 0, 't', 0
};

__code static uint8_t desc_pro[]=
{
	0x08, 0x03, 'D', 0, 'e', 0, 'v', 0
};

//Microsoft WCID descriptors
__code static uint8_t desc_osd[]=
{
	0x12, 0x03, 0x4d, 0x00, 0x53, 0x00, 0x46, 0x00,
	0x54, 0x00, 0x31, 0x00, 0x30, 0x00, 0x30, 0x00,
	0x01, 0x01
};

__code static uint8_t desc_cid[]=
{
	0x28, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x01, 0x57, 0x49, 0x4e, 0x55, 0x53, 0x42,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
