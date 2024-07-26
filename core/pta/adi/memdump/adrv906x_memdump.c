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

#include <adrv906x_util.h>

#include "adrv906x_memdump_list.h"
#include "adrv906x_memdump_exclusion_list.h"

/*
 * get_num_records - get number of records
 */
uint32_t get_num_records(void)
{
	uint32_t num_records = sizeof(memdump_primary_list) / sizeof(memdump_registers_t);

	if (plat_is_dual_tile())
		num_records += sizeof(memdump_secondary_list) / sizeof(memdump_registers_t);

	return num_records;
}

/*
 * get_record - return record for specified record number
 */
memdump_registers_t get_record(uint32_t record_num)
{
	uint32_t primary_num_records = sizeof(memdump_primary_list) / sizeof(memdump_registers_t);

	if (plat_is_dual_tile())
		if (record_num >= primary_num_records)
			return memdump_secondary_list[record_num - primary_num_records];

	return memdump_primary_list[record_num];
}

/*
 * get_bit_field_exclusion - returns bit fields which need to be cleared for a specified address
 */
uint32_t get_bit_field_exclusion(uint32_t address)
{
	uint32_t exclusion_list_size = sizeof(memdump_bit_field_exclude_list) / sizeof(memdump_bit_field_exclude_list[0]);

	/* Check if address is included in exclusion list. If included, return bit fields to exclude */
	for (unsigned long int j = 0; j < exclusion_list_size; j++)
		if (memdump_bit_field_exclude_list[j][0] == address)
			return memdump_bit_field_exclude_list[j][1];

	return 0;
}

/*
 * is_address_excluded - returns true is register should be excluded from memory dump
 */
bool is_address_excluded(uint32_t address)
{
	uint32_t exclusion_list_size = sizeof(memdump_exclude_list) / sizeof(memdump_exclude_list[0]);

	/* Check if address is included in exclusion list. If included, return true */
	for (unsigned long int j = 0; j < exclusion_list_size; j++)
		if (address >= memdump_exclude_list[j][0] && address <= memdump_exclude_list[j][1])
			return true;

	return false;
}
