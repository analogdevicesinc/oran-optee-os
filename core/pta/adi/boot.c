// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2022, Analog Devices Incorporated. All rights reserved.
 */

#include <kernel/pseudo_ta.h>
#include <mm/core_memprot.h>

#include <common.h>
#include <adrv906x_util.h>

#include <drivers/adi/adrv906x/adi_adrv906x_status_reg.h>

#define TA_NAME         "boot.ta"

#define BOOT_PTA_UUID \
	{ 0x2fd97d66, 0xe52f, 0x4e29, \
	  { 0x8e, 0x61, 0xd1, 0x86, 0xeb, 0xb4, 0x86, 0xf6 } }

#define BOOT_CMD_SET_BOOT_SUCCESSFUL            0

static TEE_Result set_boot_successful(uint32_t type)
{
	uint32_t ret;
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (type != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	ret = plat_wr_status_reg(BOOT_CNT, 0);
	if (!ret)
		return TEE_ERROR_GENERIC;

/* TODO: Enable these setters assuring we can properly write OTP memory */
#if 0
	/* Update enforcement counters */
	ret = plat_set_enforcement_counter();
	if (ret != TEE_SUCCESS)
		return ret;
	ret = plat_set_te_enforcement_counter();
	if (ret != TEE_SUCCESS)
		return ret;
#endif

	return TEE_SUCCESS;
}

/*
 * Trusted Application Entry Points
 */
static TEE_Result invoke_command(void *psess __unused,
				 uint32_t cmd, uint32_t ptypes,
				 TEE_Param params[TEE_NUM_PARAMS] __unused)
{
	switch (cmd) {
	case BOOT_CMD_SET_BOOT_SUCCESSFUL:
		return set_boot_successful(ptypes);
	default:
		break;
	}
	return TEE_ERROR_BAD_PARAMETERS;
}

pseudo_ta_register(.uuid = BOOT_PTA_UUID, .name = TA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command);
