// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024 Analog Devices Incorporated
 */

#include "adrv906x_i2c.h"

extern const i2c_entry_t adi_i2c_access_table[];
extern const size_t size_adi_i2c_access_table;

const i2c_entry_t *get_i2c_access_table(void)
{
	return adi_i2c_access_table;
}

size_t get_i2c_access_table_num_entries(void)
{
	return size_adi_i2c_access_table;
}
