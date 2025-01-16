// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025, Analog Devices Incorporated. All rights reserved.
 */

#include <common.h>
#include <kernel/pseudo_ta.h>

#include <runtime_log.h>
#include <string.h>

#define TA_NAME         "runtime_log.ta"

/*
 * This UUID is generated with uuidgen
 * the ITU-T UUID generator at http://www.itu.int/ITU-T/asn1/uuid.html
 */
#define LOG_PTA_UUID \
	{ \
		0x6dc55088, \
		0x4255, 0x41cc, \
		{ \
			0x9b, 0x49, \
			0x04, 0x53, \
			0x4e, 0x6a, \
			0xc3, 0xa6, \
		} \
	}

#define OP_PARAM_OPTEE_BUFFER 0
#define OP_PARAM_BL31_BUFFER 1

/* This value must match the value in arm-trusted-firmware/plat/adi/adrv/common/plat_runtime_log.c */
#define SIZE_OF_BL31_RUNTIME_BUFFER     500

enum ta_smc_cmds {
	BL31_RUNTIME_LOG_GET_SIZE,
	OPTEE_RUNTIME_LOG_GET_SIZE,
	RUNTIME_LOG_CMD_GET
};

/* Get size of BL31 runtime log */
static TEE_Result get_bl31_log_size(uint32_t type, TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	/* Check param types */
	if (type != exp_param_types) {
		plat_runtime_error_message("Bad parameters to get_bl31_log_size function");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Return size of BL31 runtime buffer */
	params[0].value.a = SIZE_OF_BL31_RUNTIME_BUFFER;

	return TEE_SUCCESS;
}

/* Get size of OP-TEE runtime log */
static TEE_Result get_optee_log_size(uint32_t type, TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	/* Check param types */
	if (type != exp_param_types) {
		plat_runtime_error_message("Bad parameters to get_optee_log_size function");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Return size of OP-TEE runtime buffer */
	params[0].value.a = SIZE_OF_OPTEE_RUNTIME_BUFFER;

	return TEE_SUCCESS;
}

/* Get BL31 and OP-TEE runtime logs */
static TEE_Result get_runtime_logs(uint32_t type, TEE_Param params[TEE_NUM_PARAMS])
{
	char optee_runtime_log[SIZE_OF_OPTEE_RUNTIME_BUFFER] = { 0 };
	char bl31_runtime_log[SIZE_OF_BL31_RUNTIME_BUFFER] = { 0 };
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,
						   TEE_PARAM_TYPE_MEMREF_OUTPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	/* Check param types */
	if (type != exp_param_types) {
		plat_runtime_error_message("Bad parameters to get_runtime_logs function");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Get OP-TEE runtime log */
	read_from_runtime_buffer(optee_runtime_log, sizeof(optee_runtime_log));
	memcpy(params[OP_PARAM_OPTEE_BUFFER].memref.buffer, optee_runtime_log, strlen(optee_runtime_log));

	/* SMC call to get BL31 runtime log */
	adi_runtime_log_smc(bl31_runtime_log, sizeof(bl31_runtime_log));
	memcpy(params[OP_PARAM_BL31_BUFFER].memref.buffer, bl31_runtime_log, strlen(bl31_runtime_log));

	return TEE_SUCCESS;
}

/*
 * Trusted Application Entry Points
 */
static TEE_Result invoke_command(void *psess __unused,
				 uint32_t cmd, uint32_t ptypes,
				 TEE_Param params[TEE_NUM_PARAMS])
{
	switch (cmd) {
	case BL31_RUNTIME_LOG_GET_SIZE:
		return get_bl31_log_size(ptypes, params);
	case OPTEE_RUNTIME_LOG_GET_SIZE:
		return get_optee_log_size(ptypes, params);
	case RUNTIME_LOG_CMD_GET:
		return get_runtime_logs(ptypes, params);
	default:
		break;
	}

	plat_runtime_error_message("No matching command: %d", cmd);
	return TEE_ERROR_BAD_PARAMETERS;
}

pseudo_ta_register(.uuid = LOG_PTA_UUID, .name = TA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command);
