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

#include <io.h>
#include <kernel/pseudo_ta.h>
#include <mm/core_memprot.h>

#include <stdio.h>
#include <string.h>
#include <string_ext.h>

#include <adrv906x_def.h>
#include <adrv906x_util.h>
#include <common.h>
#include <drivers/adi/adi_twi_i2c.h>

#include "adrv906x_i2c.h"

#define TA_NAME         "adi_i2c.ta"

#define TA_ADI_I2C_UUID \
	{ \
		0x7e078f09, \
		0xe8cb, 0x47ac, \
		{ \
			0xbc, 0x44, \
			0xfc, 0x6f, \
			0x09, 0x17, \
			0x43, 0x57, \
		} \
	}

#define TA_ADI_I2C_GET    0
#define TA_ADI_I2C_SET    1
#define TA_ADI_I2C_SET_GET    2

/* Op parameter offsets */
#define OP_PARAM_I2C 0
#define OP_PARAM_BUFFER 1

#define ADI_I2C_MAX_BYTES       256

typedef struct i2c_params {
	uint64_t bus;
	uint64_t slave;
	uint64_t address;
	uint64_t length;
	uint64_t set_bytes;
	uint64_t get_bytes;
	uint64_t speed;
} i2c_params_t;

static i2c_params_t i2c_params;

/*
 * init_i2c_params - initialize i2c structure to hold parameters
 */
static void init_i2c_params(TEE_Param params[TEE_NUM_PARAMS])
{
	memcpy(&i2c_params, params[0].memref.buffer, params[0].memref.size);
}

/*
 * adi_i2c_get_current_entry - get current entry if it is in the access table
 */
static const i2c_entry_t *adi_i2c_get_current_entry(void)
{
	const i2c_entry_t *access_table;
	const size_t num_entries = get_i2c_access_table_num_entries();
	const i2c_entry_t *cur_entry;
	size_t cur_entry_idx;
	uint64_t bus = i2c_params.bus;
	uint64_t slave = i2c_params.slave;
	uint64_t addr = i2c_params.address;

	access_table = get_i2c_access_table();

	for (cur_entry_idx = 0; cur_entry_idx < num_entries; cur_entry_idx++) {
		cur_entry = &(access_table[cur_entry_idx]);
		if (bus == cur_entry->bus)
			if (slave == cur_entry->slave)
				if (addr == cur_entry->address)
					return cur_entry;
	}

	return NULL;
}

/*
 * adi_i2c_verify_access - verify bus, slave, and address access
 */
static bool adi_i2c_verify_access(uint32_t cmd)
{
	const i2c_entry_t *cur_entry = adi_i2c_get_current_entry();

	if (cur_entry == NULL)
		return false;

	/* Check read, write, write/read operation */
	switch (cmd) {
	case TA_ADI_I2C_GET:
		if (cur_entry->read)
			return true;
		break;
	case TA_ADI_I2C_SET:
		if (cur_entry->write)
			return true;
		break;
	case TA_ADI_I2C_SET_GET:
		if (cur_entry->write_read)
			return true;
		break;
	default:
		break;
	}
	EMSG("Invalid operation\n");
	return false;
}

/*
 * adi_i2c_check_params - verify the received parameters are of the expected types
 */
static TEE_Result adi_i2c_check_params(uint32_t param_types, uint32_t cmd)
{
	uint64_t num_set_bytes = i2c_params.set_bytes;
	uint64_t num_get_bytes = i2c_params.get_bytes;
	uint64_t speed = i2c_params.speed;

	switch (cmd) {
	case TA_ADI_I2C_GET:
		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT, TEE_PARAM_TYPE_MEMREF_OUTPUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE)) {
			EMSG("Bad parameters\n");
			return TEE_ERROR_BAD_PARAMETERS;
		}
		break;
	case TA_ADI_I2C_SET:
		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT, TEE_PARAM_TYPE_MEMREF_INPUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE)) {
			EMSG("Bad parameters\n");
			return TEE_ERROR_BAD_PARAMETERS;
		}
		break;
	case TA_ADI_I2C_SET_GET:
		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT, TEE_PARAM_TYPE_MEMREF_INOUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE)) {
			EMSG("Bad parameters\n");
			return TEE_ERROR_BAD_PARAMETERS;
		}
		break;
	default:
		EMSG("Invalid command\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Verify I2C bus, slave, and address */
	if (!adi_i2c_verify_access(cmd)) {
		EMSG("Access not permitted for specified bus, slave, address, and operation\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Verify I2C speed */
	if ((speed < I2C_SPEED_MIN) || (speed > I2C_SPEED_MAX)) {
		EMSG("Invalid I2C speed: %ld\n", speed);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Verify buffer size is within range */
	if (num_set_bytes > ADI_I2C_MAX_BYTES || num_get_bytes > ADI_I2C_MAX_BYTES) {
		EMSG("Number of bytes specified is above the limit of %d\n", ADI_I2C_MAX_BYTES);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	return TEE_SUCCESS;
}

/*
 * i2c_set - write to I2C
 */
static TEE_Result i2c_set(TEE_Param params[TEE_NUM_PARAMS])
{
	struct adi_i2c_handle hi2c;
	uint8_t *buf = NULL;
	int ret;
	uint64_t bus = i2c_params.bus;
	uint64_t slave = i2c_params.slave;
	uint64_t addr = i2c_params.address;
	uint64_t addr_len = i2c_params.length;
	uint64_t num_bytes = i2c_params.set_bytes;
	uint64_t speed = i2c_params.speed;

	buf = malloc(num_bytes);
	if (buf == NULL) {
		EMSG("Error creating buffer\n");
		return TEE_ERROR_GENERIC;
	}

	/* Copy write data to buffer for I2C write */
	memcpy(buf, params[OP_PARAM_BUFFER].memref.buffer, num_bytes);

	/* Initialize I2C */
	hi2c.sclk = plat_get_sysclk_freq();
	hi2c.twi_clk = speed;

	switch (bus) {
	case 0:
		hi2c.pa = I2C_0_BASE;
		break;
	default:
		free(buf);
		return TEE_ERROR_GENERIC;
	}

	ret = adi_twi_i2c_init(&hi2c);
	if (ret < 0) {
		EMSG("I2C init error\n");
		free(buf);
		return TEE_ERROR_GENERIC;
	}

	/* Execute I2C write */
	ret = adi_twi_i2c_write(&hi2c, slave, addr, addr_len, buf, num_bytes);
	if (ret < 0) {
		EMSG("I2C write error\n");
		free(buf);
		return TEE_ERROR_GENERIC;
	}

	free(buf);
	return TEE_SUCCESS;
}

/*
 * i2c_get - read from I2C
 */
static TEE_Result i2c_get(TEE_Param params[TEE_NUM_PARAMS])
{
	struct adi_i2c_handle hi2c;
	uint8_t *buf = NULL;
	int ret;
	uint64_t bus = i2c_params.bus;
	uint64_t slave = i2c_params.slave;
	uint64_t addr = i2c_params.address;
	uint64_t addr_len = i2c_params.length;
	uint64_t num_bytes = i2c_params.get_bytes;
	uint64_t speed = i2c_params.speed;

	buf = malloc(num_bytes);
	if (buf == NULL) {
		EMSG("Error creating buffer\n");
		return TEE_ERROR_GENERIC;
	}

	/* Initialize I2C */
	hi2c.sclk = plat_get_sysclk_freq();
	hi2c.twi_clk = speed;

	switch (bus) {
	case 0:
		hi2c.pa = I2C_0_BASE;
		break;
	default:
		free(buf);
		return TEE_ERROR_GENERIC;
	}

	ret = adi_twi_i2c_init(&hi2c);
	if (ret < 0) {
		EMSG("I2C init error\n");
		free(buf);
		return TEE_ERROR_GENERIC;
	}

	/* Execute I2C read */
	ret = adi_twi_i2c_read(&hi2c, slave, addr, addr_len, buf, num_bytes);
	if (ret < 0) {
		EMSG("I2C read error\n");
		free(buf);
		return TEE_ERROR_GENERIC;
	}

	/* Copy data from I2C read to shared buffer */
	memcpy(params[OP_PARAM_BUFFER].memref.buffer, buf, num_bytes);

	free(buf);

	return TEE_SUCCESS;
}

/*
 * i2c_set_get - write and then read from I2C
 */
static TEE_Result i2c_set_get(TEE_Param params[TEE_NUM_PARAMS])
{
	struct adi_i2c_handle hi2c;
	uint8_t *buf = NULL;
	int ret;
	uint64_t bus = i2c_params.bus;
	uint64_t slave = i2c_params.slave;
	uint64_t addr = i2c_params.address;
	uint64_t addr_len = i2c_params.length;
	uint64_t num_get_bytes = i2c_params.get_bytes;
	uint64_t num_set_bytes = i2c_params.set_bytes;
	uint64_t speed = i2c_params.speed;
	uint64_t buf_bytes = (num_get_bytes > num_set_bytes) ? num_get_bytes : num_set_bytes;

	buf = malloc(buf_bytes);
	if (buf == NULL) {
		EMSG("Error creating buffer\n");
		return TEE_ERROR_GENERIC;
	}

	/* Copy write data to buffer for I2C write */
	memcpy(buf, params[OP_PARAM_BUFFER].memref.buffer, num_set_bytes);

	/* Initialize I2C */
	hi2c.sclk = plat_get_sysclk_freq();
	hi2c.twi_clk = speed;

	switch (bus) {
	case 0:
		hi2c.pa = I2C_0_BASE;
		break;
	default:
		free(buf);
		return TEE_ERROR_GENERIC;
	}

	ret = adi_twi_i2c_init(&hi2c);
	if (ret < 0) {
		EMSG("I2C init error\n");
		free(buf);
		return TEE_ERROR_GENERIC;
	}

	/* Execute I2C read */
	ret = adi_twi_i2c_write_read(&hi2c, slave, addr, addr_len, buf, num_set_bytes, num_get_bytes);
	if (ret < 0) {
		EMSG("I2C read error\n");
		free(buf);
		return TEE_ERROR_GENERIC;
	}

	/* Copy data from I2C read to shared buffer */
	memcpy(params[OP_PARAM_BUFFER].memref.buffer, buf, num_get_bytes);

	free(buf);

	return TEE_SUCCESS;
}

/*
 * Trusted Application Entry Points
 */
static TEE_Result invoke_command(void *psess __unused,
				 uint32_t cmd, uint32_t ptypes,
				 TEE_Param params[TEE_NUM_PARAMS])
{
	/* Initialize I2C param structure */
	init_i2c_params(params);

	/* Verify parameters */
	if (adi_i2c_check_params(ptypes, cmd) != TEE_SUCCESS)
		return TEE_ERROR_BAD_PARAMETERS;

	switch (cmd) {
	case TA_ADI_I2C_GET:
		return i2c_get(params);
	case TA_ADI_I2C_SET:
		return i2c_set(params);
	case TA_ADI_I2C_SET_GET:
		return i2c_set_get(params);
	default:
		break;
	}

	return TEE_ERROR_BAD_PARAMETERS;
}

pseudo_ta_register(.uuid = TA_ADI_I2C_UUID, .name = TA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command);
