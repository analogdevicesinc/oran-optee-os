// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024, Analog Devices Incorporated. All rights reserved.
 */

#include <kernel/pseudo_ta.h>
#include <mm/core_memprot.h>

#include <common.h>
#include <adrv906x_util.h>

#include <drivers/adi/adrv906x/adi_adrv906x_status_reg.h>

#define TA_NAME "enforcement_counter_update.ta"

/*
 * This UUID is generated with uuidgen
 * the ITU-T UUID generator at http://www.itu.int/ITU-T/asn1/uuid.html
 */
#define PTA_UUID \
	{ 0x5a3454aa, 0xdc36, 0x47bf, \
	  { 0x87, 0x0e, 0x02, 0xd8, 0x72, 0xa4, 0x75, 0xb7 } }

#define BOOT_CMD_UPDATE_ENFORCEMENT_COUNTER             0

static TEE_Result update_enforcement_counter(uint32_t type)
{
	uint32_t ret;
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (type != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	/* Update enforcement counters */
	ret = plat_set_enforcement_counter();
	if (ret != TEE_SUCCESS)
		return ret;
	ret = plat_set_te_enforcement_counter();
	if (ret != TEE_SUCCESS)
		return ret;

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
	case BOOT_CMD_UPDATE_ENFORCEMENT_COUNTER:
		return update_enforcement_counter(ptypes);
	default:
		break;
	}
	return TEE_ERROR_BAD_PARAMETERS;
}

pseudo_ta_register(.uuid = PTA_UUID, .name = TA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command);
