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

/* Op parameter offsets */
#define OP_PARAM_I2C 0
#define OP_PARAM_ADDR 1
#define OP_PARAM_SIZE 2
#define OP_PARAM_BUFFER 3

#define ADI_I2C_MAX_BYTES       256

/*
 * adi_i2c_check_params - verify the received parameters are of the expected types
 */
static TEE_Result adi_i2c_check_params(uint32_t param_types, uint32_t cmd, TEE_Param params[TEE_NUM_PARAMS])
{
	uint64_t bus = params[OP_PARAM_I2C].value.a;
	uint64_t num_bytes = params[OP_PARAM_SIZE].value.a;
	uint64_t speed = params[OP_PARAM_SIZE].value.b;

	switch (cmd) {
	case TA_ADI_I2C_GET:
		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_MEMREF_OUTPUT)) {
			EMSG("Bad parameters");
			return TEE_ERROR_BAD_PARAMETERS;
		}
		break;
	case TA_ADI_I2C_SET:
		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_MEMREF_INPUT)) {
			EMSG("Bad parameters");
			return TEE_ERROR_BAD_PARAMETERS;
		}
		break;
	default:
		EMSG("Invalid command\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Verify I2C bus */
	switch (bus) {
	case 0:
		break;
	default:
		EMSG("Bus not supported: %ld\n", bus);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Verify I2C speed */
	if ((speed < I2C_SPEED_MIN) || (speed > I2C_SPEED_MAX)) {
		EMSG("Invalid I2C speed: %ld\n", speed);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Verify buffer size is within range */
	if (num_bytes > ADI_I2C_MAX_BYTES) {
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
	uint64_t bus = params[OP_PARAM_I2C].value.a;
	uint64_t slave = params[OP_PARAM_I2C].value.b;
	uint64_t addr = params[OP_PARAM_ADDR].value.a;
	uint64_t addr_len = params[OP_PARAM_ADDR].value.b;
	uint64_t num_bytes = params[OP_PARAM_SIZE].value.a;
	uint64_t speed = params[OP_PARAM_SIZE].value.b;

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

	ret = adi_twi_i2c_init(&hi2c);
	if (ret < 0) {
		EMSG("I2C init error\n");
		free(buf);
		return TEE_ERROR_GENERIC;
	}

	switch (bus) {
	case 0:
		hi2c.pa = I2C_0_BASE;
		break;
	default:
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
	uint64_t bus = params[OP_PARAM_I2C].value.a;
	uint64_t slave = params[OP_PARAM_I2C].value.b;
	uint64_t addr = params[OP_PARAM_ADDR].value.a;
	uint64_t addr_len = params[OP_PARAM_ADDR].value.b;
	uint64_t num_bytes = params[OP_PARAM_SIZE].value.a;
	uint64_t speed = params[OP_PARAM_SIZE].value.b;

	buf = malloc(num_bytes);
	if (buf == NULL) {
		EMSG("Error creating buffer\n");
		return TEE_ERROR_GENERIC;
	}

	/* Initialize I2C */
	hi2c.sclk = plat_get_sysclk_freq();
	hi2c.twi_clk = speed;

	ret = adi_twi_i2c_init(&hi2c);
	if (ret < 0) {
		EMSG("I2C init error\n");
		free(buf);
		return TEE_ERROR_GENERIC;
	}

	switch (bus) {
	case 0:
		hi2c.pa = I2C_0_BASE;
		break;
	default:
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
 * Trusted Application Entry Points
 */
static TEE_Result invoke_command(void *psess __unused,
				 uint32_t cmd, uint32_t ptypes,
				 TEE_Param params[TEE_NUM_PARAMS])
{
	/* Verify parameters */
	if (adi_i2c_check_params(ptypes, cmd, params) != TEE_SUCCESS)
		return TEE_ERROR_BAD_PARAMETERS;

	switch (cmd) {
	case TA_ADI_I2C_GET:
		return i2c_get(params);
	case TA_ADI_I2C_SET:
		return i2c_set(params);
	default:
		break;
	}
	return TEE_ERROR_BAD_PARAMETERS;
}

pseudo_ta_register(.uuid = TA_ADI_I2C_UUID, .name = TA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command);
