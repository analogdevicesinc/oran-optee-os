/// SPDX-License-Identifier: BSD-2-Clause
/*
 * ADI ADRV906X PINCTRL Init
 *
 * Copyright (c) 2023, Analog Devices Incorporated. All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <drivers/adi/adrv906x/adi_adrv906x_pinctrl.h>
#include <drivers/adi/adrv906x/adi_adrv906x_pinmux_source_def.h>
#include <tee_api_defines.h>
#include <initcall.h>

#include <adrv906x_def.h>

#define ADI_ADRV906X_UNUSED_CONFIG        0U

/*
 * I2C PINCTRL GROUP, I2C0
 */
const pinctrl_settings i2c_pin_grp[] = {
	/*     pin#                  SRCMUX                   DS           ST     P_EN   PU        extendedOptions1         extendedOptions2*/
	{ I2C0_SCL_DIO_PIN, I2C0_SCL_DIO_MUX_SEL, CMOS_PAD_DS_0100, false, false, false, ADI_ADRV906X_UNUSED_CONFIG, ADI_ADRV906X_UNUSED_CONFIG },
	{ I2C0_SDA_DIO_PIN, I2C0_SDA_DIO_MUX_SEL, CMOS_PAD_DS_0100, false, false, false, ADI_ADRV906X_UNUSED_CONFIG, ADI_ADRV906X_UNUSED_CONFIG }
};
const size_t i2c_pin_grp_members = sizeof(i2c_pin_grp) / sizeof(pinctrl_settings);

/**
 *	Pinmux Initialization routine
 *
 */
static TEE_Result adi_adrv906x_pinctrl_init(void)
{
	if (!adi_adrv906x_pinctrl_set_group(i2c_pin_grp, i2c_pin_grp_members, PINCTRL_BASE))
		return TEE_ERROR_GENERIC;

	return TEE_SUCCESS;
}

preinit(adi_adrv906x_pinctrl_init);
