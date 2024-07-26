// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024, Analog Devices Incorporated. All rights reserved.
 */

#ifndef ADI_MEMDUMP_H
#define ADI_MEMDUMP_H

#include <stdint.h>
#include <stdbool.h>

typedef struct cpu_mem_dump {
	uint32_t cpu_mem_addr;
	uint32_t cpu_mem_size;
	uint8_t cpu_mem_width;
	uint8_t cpu_mem_endianness;
} memdump_registers_t;

uint32_t get_num_records(void);
memdump_registers_t get_record(uint32_t record_num);
uint32_t get_bit_field_exclusion(uint32_t address);
bool is_address_excluded(uint32_t address);

#endif /* ADI_MEMDUMP_H */
