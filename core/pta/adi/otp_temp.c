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
#include <stdio.h>

#include <io.h>
#include <kernel/pseudo_ta.h>
#include <mm/core_memprot.h>
#include <tee_internal_api.h>
#include <adrv906x_def.h>
#include <drivers/adi/adi_otp.h>
#include <drivers/adi/adrv906x/adi_adrv906x_otp.h>

#define TA_NAME "otp_temp.ta"

/*
 * This UUID is generated with uuidgen
 * the ITU-T UUID generator at http://www.itu.int/ITU-T/asn1/uuid.html
 */
#define TA_OTP_TEMP_UUID \
	{ \
		0xcf0ba31d, \
		0xa0a8, 0x4406, \
		{ \
			0x9e, 0x8c, \
			0xba, 0x11, \
			0xdf, 0x80, \
			0xfb, 0xb1, \
		} \
	}

/* Op parameter offsets */
#define OP_PARAM_TEMP_GROUP_ID   0
#define OP_PARAM_TEMP_VALUE      1

/* The function IDs implemented in this TA */
enum ta_otp_temp_cmds {
	TA_OTP_TEMP_CMD_READ,
	TA_OTP_TEMP_CMD_WRITE,
	TA_OTP_TEMP_CMDS_COUNT
};

/*
 * otp_temp_check_params - verify the received parameters are of the expected types, and MAC id parameter is valid
 */
static TEE_Result otp_temp_check_params(uint32_t param_types, TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_VALUE_INOUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);
	uint8_t temp_group_id;

	if (param_types != exp_param_types) {
		EMSG("%s Bad parameters", TA_NAME);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	temp_group_id = params[OP_PARAM_TEMP_GROUP_ID].value.a;
	if (temp_group_id >= TEMP_SENSOR_OTP_SLOT_NUM) {
		EMSG("%s Invalid temp group id '%u'", TA_NAME, temp_group_id);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	return TEE_SUCCESS;
}

/*
 * otp_temp_read_handler - manages the read requests
 */
static TEE_Result otp_temp_read_handler(TEE_Param params[TEE_NUM_PARAMS])
{
	vaddr_t base;
	adrv906x_temp_group_id_t temp_group_id = (adrv906x_temp_group_id_t)params[OP_PARAM_TEMP_GROUP_ID].value.a;
	uint32_t value;

	bool base_is_new_mmu_map = false;
	int ret;

	base = (vaddr_t)phys_to_virt_io(OTP_BASE, SMALL_PAGE_SIZE);
	if (!base) {
		/* MMU add mapping, to support addresses not registered with "register_phys_mem" */
		base = (vaddr_t)core_mmu_add_mapping(MEM_AREA_IO_SEC, OTP_BASE, SMALL_PAGE_SIZE);
		if (!base) {
			EMSG("%s READ MMU address mapping failure", TA_NAME);
			return TEE_ERROR_GENERIC;
		}
		base_is_new_mmu_map = true;
	}

	ret = adrv906x_otp_get_temp_sensor(base, temp_group_id, &value);

	if (base_is_new_mmu_map) {
		/* MMU remove mapping */
		if (core_mmu_remove_mapping(MEM_AREA_IO_SEC, (void *)base, SMALL_PAGE_SIZE) != TEE_SUCCESS) {
			EMSG("%s READ MMU address unmapping failure", TA_NAME);
			return TEE_ERROR_GENERIC;
		}
	}

	if (ret != ADI_OTP_SUCCESS)
		return TEE_ERROR_GENERIC;

	params[OP_PARAM_TEMP_VALUE].value.a = value;
	IMSG("%s value read back: 0x%x", TA_NAME, value);

	return TEE_SUCCESS;
}

/*
 * Trusted Application Entry Points
 */
static TEE_Result invoke_command(void *psess __unused,
				 uint32_t cmd, uint32_t ptypes,
				 TEE_Param params[TEE_NUM_PARAMS])
{
	/* Check command */
	if (cmd != TA_OTP_TEMP_CMD_READ && cmd != TA_OTP_TEMP_CMD_WRITE) {
		EMSG("Invalid command");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Check parameters */
	if (otp_temp_check_params(ptypes, params) != TEE_SUCCESS)
		return TEE_ERROR_BAD_PARAMETERS;

	switch (cmd) {
	case TA_OTP_TEMP_CMD_READ:  return otp_temp_read_handler(params);
	case TA_OTP_TEMP_CMD_WRITE: IMSG("%s : write operation is not supported", TA_NAME);
	default: break;
	}
	return TEE_ERROR_BAD_PARAMETERS;
}

pseudo_ta_register(.uuid = TA_OTP_TEMP_UUID, .name = TA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command);
