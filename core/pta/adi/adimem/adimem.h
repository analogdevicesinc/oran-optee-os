// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024, Analog Devices Incorporated. All rights reserved.
 */

#ifndef ADIMEM_H
#define ADIMEM_H

#include <stddef.h>
#include <stdint.h>

#define ADIMEM_ACCESS_TYPE_READ 0x1
#define ADIMEM_ACCESS_TYPE_WRITE 0x2

typedef uint8_t adimem_access_t;

typedef struct adimem_entry {
	uint32_t address;
	size_t size;
	adimem_access_t type;
} adimem_entry_t;

const adimem_entry_t * get_access_table(void);
size_t get_access_table_num_entries(void);

#endif /* ADIMEM_H */
