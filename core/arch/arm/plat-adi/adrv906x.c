// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2022 Analog Devices Incorporated
 */

#include <common.h>
#include <drivers/pl011.h>
#include <kernel/boot.h>
#include <libfdt.h>
#include <mm/core_memprot.h>
#include <mm/core_mmu.h>
#include <platform_config.h>

#include <adrv906x_def.h>
#include <adrv906x_util.h>

/*
 * Register the large physical memory area for Secure peripherals including UART, I2C, SCRATCH regs, gpio, etc
 */
register_phys_mem(MEM_AREA_IO_SEC, ADI_ADRV906X_PERIPHERAL_BASE, ADI_ADRV906X_PERIPHERAL_SIZE);

/*
 * Register the same area for secure peripherals on the secondary tile
 * Note that this region will be removed in main_init_gic() below if it is determined
 * this is not a dual-tile system.
 */
register_phys_mem(MEM_AREA_IO_SEC, ADI_ADRV906X_SEC_PERIPHERAL_BASE, ADI_ADRV906X_SEC_PERIPHERAL_SIZE);

/*
 * Registering the non-secure console
 */
register_phys_mem(MEM_AREA_IO_NSEC, PL011_0_BASE, PL011_REG_SIZE);

/* Register the physical memory area for SCRATCH_NS registers */
register_phys_mem(MEM_AREA_IO_NSEC, A55_SYS_CFG + SCRATCH_NS, SMALL_PAGE_SIZE);

static bool is_dual_tile = false;
static bool is_secondary_linux_enabled = false;

/* Read the dual-tile flag from the device tree, store locally */
static void init_dual_tile_flag(void)
{
	void *fdt;
	int offset;
	const fdt32_t *prop;

	is_dual_tile = false;

	fdt = get_external_dt();
	if (fdt != 0) {
		offset = fdt_path_offset(fdt, "/boot");
		if (offset >= 0) {
			prop = fdt_getprop(fdt, offset, "dual-tile", NULL);
			if (prop != NULL)
				if (fdt32_to_cpu(*prop) == 1)
					is_dual_tile = true;
		}
	}
}

/* Read the secondary-linux-enabled flag from the device tree, store locally */
static void init_secondary_linux_flag(void)
{
	void *fdt;
	int offset;
	const fdt32_t *prop;

	is_secondary_linux_enabled = false;

	fdt = get_external_dt();
	if (fdt != 0) {
		offset = fdt_path_offset(fdt, "/boot");
		if (offset >= 0) {
			prop = fdt_getprop(fdt, offset, "secondary-linux-enabled", NULL);
			if (prop != NULL)
				if (fdt32_to_cpu(*prop) == 1)
					is_secondary_linux_enabled = true;
		}
	}
}

/* Return the cached copy of the dual-tile flag from the device tree */
bool plat_is_dual_tile(void)
{
	return is_dual_tile;
}

/* Return the cached copy of the secondary-linux-enabled flag from the device tree */
bool plat_is_secondary_linux_enabled(void)
{
	return is_secondary_linux_enabled;
}

void main_init_gic(void)
{
	vaddr_t addr;
	TEE_Result ret;

	/* Read and cache the dual-tile and secondary-linux-enabled flags from the device tree.
	 * Note: It is necessary to save these off here because the device tree is
	 * unavailable at runtime, when the secondary_launcher PTA needs this information.
	 */
	init_dual_tile_flag();
	init_secondary_linux_flag();

	/* If this is not a dual-tile system, remove the page table entry for secondary peripherals.
	 * Note: It is easier to remove an entry for single-tile than dynamically add an entry
	 * for dual-tile because of dynamic entry size restrictions.
	 */
	if (!plat_is_dual_tile()) {
		addr = (vaddr_t)phys_to_virt(ADI_ADRV906X_SEC_PERIPHERAL_BASE, MEM_AREA_IO_SEC, ADI_ADRV906X_SEC_PERIPHERAL_SIZE);
		ret = core_mmu_remove_mapping(MEM_AREA_IO_SEC, (void *)addr, ADI_ADRV906X_SEC_PERIPHERAL_SIZE);
		if (ret != TEE_SUCCESS)
			IMSG("WARNING: Unable to remove secondary peripheral page table entry\n");
	}

	/* Do common initialization */
	common_main_init_gic();
}
