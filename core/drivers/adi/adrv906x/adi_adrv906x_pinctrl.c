// SPDX-License-Identifier: BSD-2-Clause
/*
 * ADI ADRV906X PINCTRL Driver
 *
 * Copyright (c) 2023, Analog Devices Incorporated. All rights reserved.
 */

#include <stdbool.h>
#include <stddef.h>

#include <kernel/thread.h>

#include <drivers/adi/adrv906x/adi_adrv906x_pinctrl.h>
#include <drivers/adi/adrv906x/adi_adrv906x_pinmux_source_def.h>

/* PINCTRL Function ID*/
#define ADI_PINCTRL_SIP_SERVICE_FUNCTION_ID                     0xC2000001

/* ADI Pinctrl SIP Service Functions*/
#define ADI_PINCTRL_INIT (0U)
#define ADI_PINCTRL_SET  (1U)
#define ADI_PINCTRL_GET  (2U)

/* SMC Config Bitfield Config Word */
#define ADI_BITFIELD_ST_BIT_POSITION                    (0U)
#define ADI_BITFIELD_PULL_ENABLEMENT_BIT_POSITION       (1U)
#define ADI_BITFIELD_PULLUP_ENABLE_BIT_POSITION         (2U)
#define ADI_CONFIG_DRIVE_STRENGTH_MASK                  (0x0000000FU)
#define ADI_CONFIG_DRIVE_STRENGTH_MASK_BIT_POSITION     (0U)

/* SMC Handler return Status Values (res.a0 return value) */
#define ADI_PINCTRL_SMC_RETURN_SUCCESS                  (0U)
#define ADI_PINCTRL_SMC_RETURN_UNSUPPORTED_REQUEST      (0xFFFFFFFFFFFFFFFFU)

/* SMC Pinctrl Handler return values (res.a1 return value) */
#define ADI_TFA_PINCTRL_HANDLER_FAILURE                 (0U)
#define ADI_TFA_PINCTRL_HANDLER_SUCCESS                 (1U)


static bool adi_pinconf_set_smc(const pinctrl_settings settings, uintptr_t base_addr)
{
	int drive_strength;
	int schmitt_trig_enable;
	int pin_pull_enablement;
	int pin_pull_up_enable;
	int config_bitfield;
	bool pin_is_dio = false;

	struct thread_smc_args args;

	if (settings.pin_pad >= ADRV906X_DIO_PIN_START && settings.pin_pad < (ADRV906X_DIO_PIN_START + ADRV906X_DIO_PIN_COUNT))
		pin_is_dio = true;

	if (settings.pin_pad >= ADRV906X_PIN_COUNT && !pin_is_dio)
		return false;

	/*
	 * Setup  smc call to perform the pinconf_set operation
	 *
	 * thread_smc_args expected params:
	 *    a0: SMC SIP SERVICE ID
	 *    a1: ADI Pinctrl request (GET, SET, INIT)
	 *    a2: Pin Number requested
	 *    a3: Source Mux setting requested
	 *    a4: Drive Strength
	 *    a5: BIT_FIELD-3bits-(SchmittTrigEnable | PU PD Enablement | PU Enable)
	 *    a6: Base Address
	 *    a7: Currently UNUSED/UNDEFINED
	 *
	 * Return Params
	 *    a0: SMC return value
	 *    a1: ADI TFA Pinctrl Handler return status
	 */

	drive_strength = settings.drive_strength & ADI_CONFIG_DRIVE_STRENGTH_MASK;
	schmitt_trig_enable = settings.schmitt_trigger_enable ? 1 : 0;
	pin_pull_enablement = settings.pullup_pulldown_enablement ? 1 : 0;
	pin_pull_up_enable = settings.pullup ? 1 : 0;
	config_bitfield = (schmitt_trig_enable << ADI_BITFIELD_ST_BIT_POSITION) |
			  (pin_pull_enablement << ADI_BITFIELD_PULL_ENABLEMENT_BIT_POSITION) |
			  (pin_pull_up_enable << ADI_BITFIELD_PULLUP_ENABLE_BIT_POSITION);

	args.a0 = ADI_PINCTRL_SIP_SERVICE_FUNCTION_ID;
	args.a1 = ADI_PINCTRL_SET;
	args.a2 = settings.pin_pad;
	args.a3 = settings.src_mux;
	args.a4 = drive_strength;
	args.a5 = config_bitfield;
	args.a6 = base_addr;

	thread_smccc(&args);

	if (args.a0 != ADI_PINCTRL_SMC_RETURN_SUCCESS || args.a1 != ADI_TFA_PINCTRL_HANDLER_SUCCESS) {
		EMSG("OPTEE :: adi_pinconf_set_smc :: args.a0 == ADI_SECURE_PINCTRL_SMC\n");
		return false;
	}

	return true;
}

/**
 *	Pinmux set function, returns true if set command completes successfully, else false
 *		all configuration parameters within the settings parameter, secure_access = true
 * 		when request originates from secure_world.
 *
 */
bool adi_adrv906x_pinctrl_set(const pinctrl_settings settings, uintptr_t base_addr)
{
	return adi_pinconf_set_smc(settings, base_addr);
}

/**
 *	Pinmux set group function, for use by secure_world software
 *		configures groups of I/O defined by the incoming pinctrl_settings array
 *		returns true upon success.
 *
 */
bool adi_adrv906x_pinctrl_set_group(const pinctrl_settings pin_group_settings[], const size_t pin_grp_members, uintptr_t base_addr)
{
	size_t group_member;

	if (pin_group_settings == NULL || pin_grp_members == 0U)
		return false;

	for (group_member = 0U; group_member < pin_grp_members; group_member++)
		if (!adi_adrv906x_pinctrl_set(pin_group_settings[group_member], base_addr))
			return false;

	return true;
}
