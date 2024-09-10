// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2022, Analog Devices Incorporated. All rights reserved.
 */

#include <io.h>
#include <mm/core_memprot.h>

#include <drivers/adi/adrv906x/adi_adrv906x_status_reg.h>

/* These values MUST MATCH the implementation the following repos
 *
 * U-boot: /arch/arm/mach-adrv906x/adrv906x_status_reg.c
 * Linux: /drivers/soc/adi/adrv906x-err.c
 * TF-A: /plat/adi/adrv/adrv906x/adrv906x_status_reg.c
 */
#define RESET_CAUSE_OFFSET              0
#define BOOT_CNT_OFFSET         4

/*
 * List of reasons reset was performed which gets stored in RESET_CAUSE
 * This enum MUST MATCH those defined in the following repos
 *
 * U-boot: /arch/arm/mach-adrv906x/include/plat_status_reg.h
 * Linux: /drivers/soc/adi/adrv906x-err.c
 * TF-A: /plat/adi/adrv/common/include/plat_status_reg.h
 */
typedef enum {
	RESET_VALUE,
	IMG_VERIFY_FAIL,
	WATCHDOG_RESET,
	CACHE_ECC_ERROR,
	DRAM_ECC_ERROR,
	OTHER_RESET_CAUSE,
} reset_cause_t;

/* Read from specified boot status register */
uint32_t plat_rd_status_reg(plat_status_reg_id_t reg)
{
	vaddr_t rng;

	switch (reg) {
	case RESET_CAUSE:
		rng = (vaddr_t)phys_to_virt_io(A55_SYS_CFG + SCRATCH, SMALL_PAGE_SIZE);
		if (!rng)
			return 0;
		return io_read32(rng + RESET_CAUSE_OFFSET);

	case BOOT_CNT:
		rng = (vaddr_t)phys_to_virt_io(A55_SYS_CFG + SCRATCH, SMALL_PAGE_SIZE);
		if (!rng)
			return 0;
		return io_read32(rng + BOOT_CNT_OFFSET);

	default:
		IMSG("Not a valid status register\n");
		return 0;
	}
}

/* Write value to specified boot status register */
bool plat_wr_status_reg(plat_status_reg_id_t reg, uint32_t value)
{
	vaddr_t rng;

	switch (reg) {
	case RESET_CAUSE:
		rng = (vaddr_t)phys_to_virt_io(A55_SYS_CFG + SCRATCH, SMALL_PAGE_SIZE);
		if (!rng)
			return false;
		io_write32(rng + RESET_CAUSE_OFFSET, value);
		break;

	case BOOT_CNT:
		rng = (vaddr_t)phys_to_virt_io(A55_SYS_CFG + SCRATCH, SMALL_PAGE_SIZE);
		if (!rng)
			return false;
		io_write32(rng + BOOT_CNT_OFFSET, value);
		break;

	default:
		IMSG("Not a valid status register\n");
		return false;
	}
	return true;
}
