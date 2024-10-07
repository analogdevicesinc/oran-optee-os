// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024 Analog Devices Incorporated
 */

#include "adrv906x_i2c.h"

#define ADM1266 0x41
#define MAX20860_0      0x24
#define MAX20860_1      0x25

const i2c_entry_t adi_i2c_access_table[] =
{
	{ 0, ADM1266,	 0xE8, true,  false, false },
	{ 0, ADM1266,	 0xE7, true,  false, false },
	{ 0, ADM1266,	 0xF0, false, true,  true  },
	{ 0, ADM1266,	 0xF1, false, true,  true  },
	{ 0, MAX20860_0, 0x88, true,  false, false },
	{ 0, MAX20860_0, 0x8B, true,  false, false },
	{ 0, MAX20860_0, 0x8C, true,  false, false },
	{ 0, MAX20860_0, 0x7A, true,  false, false },
	{ 0, MAX20860_0, 0x7B, true,  false, false },
	{ 0, MAX20860_1, 0x88, true,  false, false },
	{ 0, MAX20860_1, 0x8B, true,  false, false },
	{ 0, MAX20860_1, 0x8C, true,  false, false },
	{ 0, MAX20860_1, 0x7A, true,  false, false },
	{ 0, MAX20860_1, 0x7B, true,  false, false }
};

const size_t size_adi_i2c_access_table = sizeof(adi_i2c_access_table) / sizeof(i2c_entry_t);
