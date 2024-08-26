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

#include "adimem.h"

/* Set of non-privileged accesses allowed on ADRV906X */
static const adimem_entry_t adimem_entry_table[] =
{
	{ 0x18291658, 8,  ADIMEM_ACCESS_TYPE_READ			     },
	{ 0x18291664, 8,  ADIMEM_ACCESS_TYPE_READ			     },
	{ 0x20103100, 32, ADIMEM_ACCESS_TYPE_READ | ADIMEM_ACCESS_TYPE_WRITE },
	{ 0x20103104, 32, ADIMEM_ACCESS_TYPE_READ | ADIMEM_ACCESS_TYPE_WRITE },
	{ 0x20103108, 32, ADIMEM_ACCESS_TYPE_READ			     }
};

const adimem_entry_t * get_access_table(void)
{
	return adimem_entry_table;
}

size_t get_access_table_num_entries(void)
{
	return sizeof(adimem_entry_table) / sizeof(adimem_entry_t);
}
