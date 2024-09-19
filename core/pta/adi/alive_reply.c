// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024, Analog Devices Incorporated. All rights reserved.
 */

#include <kernel/pseudo_ta.h>

#define TA_NAME "alive_reply.ta"

#define ALIVE_REPLY_PTA_UUID \
	{ 0xafbc7ee1, 0x8a5c, 0x4d59, \
	  { 0x89, 0xe1, 0xe1, 0x95, 0x40, 0xf7, 0xf9, 0x83 } }

/*
 * Trusted Application Entry Points
 */
static TEE_Result invoke_command(void *psess __unused,
				 uint32_t cmd __unused,
				 uint32_t ptypes __unused,
				 TEE_Param params[TEE_NUM_PARAMS] __unused)
{
	return TEE_SUCCESS;
}

pseudo_ta_register(.uuid = ALIVE_REPLY_PTA_UUID,
		   .name = TA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command);
