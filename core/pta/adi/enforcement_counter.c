/*
 * Copyright (c) 2024, Analog Devices Incorporated - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include <kernel/pseudo_ta.h>
#include <mm/core_memprot.h>

#include <adrv906x_def.h>
#include <adrv906x_util.h>

#define TA_NAME "enforcement_counter.ta"

/*
 * This UUID is generated with uuidgen
 * the ITU-T UUID generator at http://www.itu.int/ITU-T/asn1/uuid.html
 */
#define ENFORCEMENT_COUNTER_PTA_UUID \
	{ 0xf20f1c1c, 0x2d8c, 0x4c8b, \
	  { 0xa9, 0xf7, 0xbf, 0x74, 0xae, 0x80, 0xcf, 0x1f } }


#define CMD_GET_ENFORCEMENT_COUNTER        0
#define CMD_GET_TE_ENFORCEMENT_COUNTER     1

/*
 * Trusted Application Entry Points
 */
static TEE_Result invoke_command(void *psess __unused,
				 uint32_t cmd, uint32_t ptypes,
				 TEE_Param params[TEE_NUM_PARAMS] __unused)
{
	switch (cmd) {
	case CMD_GET_ENFORCEMENT_COUNTER:
		return plat_get_enforcement_counter(ptypes, params);
	case CMD_GET_TE_ENFORCEMENT_COUNTER:
		return plat_get_te_enforcement_counter(ptypes, params);
	default:
		break;
	}
	return TEE_ERROR_BAD_PARAMETERS;
}

pseudo_ta_register(.uuid = ENFORCEMENT_COUNTER_PTA_UUID, .name = TA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command);
