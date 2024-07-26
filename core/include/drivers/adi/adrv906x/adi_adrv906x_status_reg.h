// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2022, Analog Devices Incorporated. All rights reserved.
 */

/* This enum must match the enumeration found in the following repos
 *
 * U-boot: arch/arm/mach-adrv906x/include/plat_status_reg.h
 * TF-A: /plat/adi/adrv/common/include/plat_status_reg.h
 */
typedef enum {
	RESET_CAUSE_NS,
	RESET_CAUSE,
	BOOT_CNT,
	STARTING_SLOT,
	LAST_SLOT
} plat_status_reg_id_t;

uint32_t plat_rd_status_reg(plat_status_reg_id_t reg);
bool plat_wr_status_reg(plat_status_reg_id_t reg, uint32_t value);
