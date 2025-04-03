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

#include <drivers/adi/adi_otp.h>
#include <drivers/adi/adi_te_interface.h>
#include <drivers/adi/adrv906x/adi_adrv906x_otp.h>

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

/* Register the physical memory area for OTP registers */
register_phys_mem(MEM_AREA_IO_SEC, OTP_BASE, SMALL_PAGE_SIZE);

/* Register the physical memory area for OTP registers on the secondary tile */
register_phys_mem(MEM_AREA_IO_SEC, SEC_OTP_BASE, SMALL_PAGE_SIZE);

static bool is_dual_tile = false;
static bool is_secondary_linux_enabled = false;
static uint64_t sysclk_freq = 0ULL;

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

static void init_sysclk_freq(void)
{
	void *fdt;
	int offset;
	const fdt32_t *prop;

	sysclk_freq = 0;

	fdt = get_external_dt();
	if (fdt != 0) {
		offset = fdt_path_offset(fdt, "/sysclk");
		if (offset >= 0) {
			prop = fdt_getprop(fdt, offset, "clock-frequency", NULL);
			if (prop != NULL)
				sysclk_freq = fdt32_to_cpu(*prop);
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

TEE_Result plat_set_enforcement_counter(void)
{
	vaddr_t base;
	uint32_t current_counter_value;
	uint32_t counter_value = plat_get_anti_rollback_counter();

	base = (vaddr_t)phys_to_virt_io(OTP_BASE, SMALL_PAGE_SIZE);

	if (adrv906x_otp_get_rollback_counter(base, &current_counter_value) != ADI_OTP_SUCCESS)
		return TEE_ERROR_GENERIC;

	/* Check device-tree value with the current value from the OTP memory */
	if (counter_value < current_counter_value)
		return TEE_ERROR_GENERIC;
	else if (counter_value == current_counter_value)
		return TEE_SUCCESS;

	if (adrv906x_otp_set_rollback_counter(base, counter_value) != ADI_OTP_SUCCESS)
		return TEE_ERROR_GENERIC;

	return TEE_SUCCESS;
}

TEE_Result plat_set_te_enforcement_counter(void)
{
	int status = 0;
	uint32_t current_counter_value;
	uint32_t new_counter_value = plat_get_te_anti_rollback_counter();

	status = adi_enclave_get_otp_app_anti_rollback(TE_MAILBOX_BASE, &current_counter_value);
	if (status != 0)
		return TEE_ERROR_GENERIC;

	/* Check device-tree value with the current value from the TE OTP memory */
	if (new_counter_value < current_counter_value)
		return TEE_ERROR_GENERIC;
	else if (new_counter_value == current_counter_value)
		return TEE_SUCCESS;

	/* Run the update interface (that increments +1) as needed */
	while (new_counter_value > current_counter_value) {
		status = adi_enclave_update_otp_app_anti_rollback(TE_MAILBOX_BASE, &current_counter_value);
		if (status != 0)
			return TEE_ERROR_GENERIC;
	}

	return TEE_SUCCESS;
}

TEE_Result plat_get_enforcement_counter(uint32_t param_types, TEE_Param params[TEE_NUM_PARAMS])
{
	vaddr_t base;
	uint32_t counter_value;
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	base = (vaddr_t)phys_to_virt_io(OTP_BASE, SMALL_PAGE_SIZE);
	if (adrv906x_otp_get_rollback_counter(base, &counter_value) != ADI_OTP_SUCCESS)
		return TEE_ERROR_GENERIC;

	params[0].value.a = counter_value;

	return TEE_SUCCESS;
}

TEE_Result plat_get_te_enforcement_counter(uint32_t param_types, TEE_Param params[TEE_NUM_PARAMS])
{
	int status = 0;
	uint32_t counter_value;
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	status = adi_enclave_get_otp_app_anti_rollback(TE_MAILBOX_BASE, &counter_value);
	if (status != 0)
		return TEE_ERROR_GENERIC;

	params[0].value.a = counter_value;

	return TEE_SUCCESS;
}

/* Return the cached copy of the sysclk frequency from the device tree */
uint32_t plat_get_sysclk_freq(void)
{
	return sysclk_freq;
}

void main_init_gic(void)
{
	vaddr_t addr;
	TEE_Result ret;

	/* Read and cache the dual-tile and secondary-linux-enabled flags from the device tree.
	 * Also read and cache the sysclk frequency from the device tree.
	 * Note: It is necessary to save these off here because the device tree is
	 * unavailable at runtime, when the secondary_launcher PTA needs this information.
	 */
	init_dual_tile_flag();
	init_secondary_linux_flag();
	init_sysclk_freq();

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
