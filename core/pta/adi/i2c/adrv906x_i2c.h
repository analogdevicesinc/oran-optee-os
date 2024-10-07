/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2024 Analog Devices Incorporated
 */

#ifndef ADRV906X_I2C_H
#define ADRV906X_I2C_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct i2c_entry {
	uint64_t bus;
	uint64_t slave;
	uint64_t address;
	bool read;
	bool write;
	bool write_read;
} i2c_entry_t;

const i2c_entry_t *get_i2c_access_table(void);
size_t get_i2c_access_table_num_entries(void);

#endif /* ADRV906X_I2C_H */
