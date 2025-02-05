// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025, Analog Devices Incorporated. All rights reserved.
 */

#include <io.h>
#include <kernel/pseudo_ta.h>
#include <mm/core_memprot.h>

#include <adrv906x_util.h>
#include <common.h>

#define TA_NAME         "secondary_launcher.ta"

#define SECONDARY_LAUNCHER_PTA_UUID \
	{ \
		0xfb27d3c0, \
		0x0f18, 0x4882, \
		{ \
			0x8e, 0x2f, \
			0xcd, 0x52, \
			0x39, 0xae, \
			0x1e, 0x7a, \
		} \
	}

#define SECONDARY_LAUNCHER_CMD_BOOT_SECONDARY    0

static TEE_Result set_boot_successful(void)
{
	vaddr_t addr;
	uint32_t reg;

	if (plat_is_dual_tile()) {
		if (plat_is_secondary_linux_enabled()) {
			IMSG("Initiating secondary boot...\n");
			addr = (vaddr_t)phys_to_virt_io(SEC_A55_SYS_CFG + HOST_BOOT_OFFSET, SMALL_PAGE_SIZE);
			if (!addr)
				return TEE_ERROR_GENERIC;
			reg = io_read32(addr);
			reg |= HOST_BOOT_READY_MASK;
			io_write32(addr, reg);
			IMSG("Done\n");
		} else {
			plat_runtime_error_message("Refusing to initiate secondary boot. Not configured to boot Linux on secondary tile.");
			return TEE_ERROR_GENERIC;
		}
	} else {
		plat_runtime_error_message("Refusing to initiate secondary boot. Not a dual-tile system.");
		return TEE_ERROR_GENERIC;
	}

	return TEE_SUCCESS;
}

/*
 * Trusted Application Entry Points
 */
static TEE_Result invoke_command(void *psess __unused,
				 uint32_t cmd, uint32_t ptypes __unused,
				 TEE_Param params[TEE_NUM_PARAMS] __unused)
{
	switch (cmd) {
	case SECONDARY_LAUNCHER_CMD_BOOT_SECONDARY:
		return set_boot_successful();
	default:
		break;
	}
	return TEE_ERROR_BAD_PARAMETERS;
}

pseudo_ta_register(.uuid = SECONDARY_LAUNCHER_PTA_UUID, .name = TA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command);
