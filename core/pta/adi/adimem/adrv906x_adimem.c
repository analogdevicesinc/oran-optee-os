/*
 * Copyright (c) 2024, Analog Devices Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <adrv906x_def.h>
#include "adimem.h"

/* Set of non-privileged accesses allowed on ADRV906X */
static const adimem_entry_t adimem_entry_table[] =
{
	{ MBIAS_IGEN_RTRIM_RB0,	     8,	 ADIMEM_ACCESS_TYPE_READ			    },
	{ MBIAS_IGEN_RTRIM_RB1,	     8,	 ADIMEM_ACCESS_TYPE_READ			    },
	{ A55_SYS_CFG_PINTSWSET,     32, ADIMEM_ACCESS_TYPE_READ | ADIMEM_ACCESS_TYPE_WRITE },
	{ A55_SYS_CFG_PINTSWCLR,     32, ADIMEM_ACCESS_TYPE_READ | ADIMEM_ACCESS_TYPE_WRITE },
	{ A55_SYS_CFG_PINTSW,	     32, ADIMEM_ACCESS_TYPE_READ			    },
	{ SEC_MBIAS_IGEN_RTRIM_RB0,  8,	 ADIMEM_ACCESS_TYPE_READ			    },
	{ SEC_MBIAS_IGEN_RTRIM_RB1,  8,	 ADIMEM_ACCESS_TYPE_READ			    },
	{ SEC_A55_SYS_CFG_PINTSWSET, 32, ADIMEM_ACCESS_TYPE_READ | ADIMEM_ACCESS_TYPE_WRITE },
	{ SEC_A55_SYS_CFG_PINTSWCLR, 32, ADIMEM_ACCESS_TYPE_READ | ADIMEM_ACCESS_TYPE_WRITE },
	{ SEC_A55_SYS_CFG_PINTSW,    32, ADIMEM_ACCESS_TYPE_READ			    },
	{ ETHPLL_LOCKDET_CONFIG,     8,	 ADIMEM_ACCESS_TYPE_READ			    },
	{ CLKPLL_LOCKDET_CONFIG,     8,	 ADIMEM_ACCESS_TYPE_READ			    },
	{ SEC_ETHPLL_LOCKDET_CONFIG, 8,	 ADIMEM_ACCESS_TYPE_READ			    },
	{ SEC_CLKPLL_LOCKDET_CONFIG, 8,	 ADIMEM_ACCESS_TYPE_READ			    },
};

const adimem_entry_t * get_access_table(void)
{
	return adimem_entry_table;
}

size_t get_access_table_num_entries(void)
{
	return sizeof(adimem_entry_table) / sizeof(adimem_entry_t);
}
