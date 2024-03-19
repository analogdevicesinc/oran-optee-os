/*
 * Copyright (c) 2023, Analog Devices Incorporated, All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __ADI_ADRV906X_PINCTRL_H__
#define __ADI_ADRV906X_PINCTRL_H__

#include <io.h>
#include <stdbool.h>
#include <stdint.h>

#include <drivers/adi/adrv906x/adi_adrv906x_pinmux_source_def.h>

#define ADI_ADRV906X_UNUSED_CONFIG        0U

/** The following defines the possible ADRV906X source mux selections*/
#define ADI_PINMUX_SRC_SEL_0    0U
#define ADI_PINMUX_SRC_SEL_1    1U
#define ADI_PINMUX_SRC_SEL_2    2U
#define ADI_PINMUX_SRC_SEL_3    3U
#define ADI_PINMUX_SRC_SEL_4    4U
#define ADI_PINMUX_SRC_NONE             0xFFFFFFFFU

/* ADRV906X pad drive strength enum */
typedef enum {
	CMOS_PAD_DS_0000	= 0,
	CMOS_PAD_DS_0001	= 1,
	CMOS_PAD_DS_0010	= 2,
	CMOS_PAD_DS_0011	= 3,
	CMOS_PAD_DS_0100	= 4,
	CMOS_PAD_DS_0101	= 5,
	CMOS_PAD_DS_0110	= 6,
	CMOS_PAD_DS_0111	= 7,
	CMOS_PAD_DS_1000	= 8,
	CMOS_PAD_DS_1001	= 9,
	CMOS_PAD_DS_1010	= 10,
	CMOS_PAD_DS_1011	= 11,
	CMOS_PAD_DS_1100	= 12,
	CMOS_PAD_DS_1101	= 13,
	CMOS_PAD_DS_1110	= 14,
	CMOS_PAD_DS_1111	= 15,
} adrv906x_cmos_pad_ds_t;

/*Pull up / Pull down enum */
typedef enum {
	PULL_DOWN	= 0,
	PULL_UP		= 1,
} adrv906x_pad_pupd_t;


typedef struct {
	uint32_t pin_pad;                                               /**		Pin (or pad) Number to configured */
	uint32_t src_mux;                                               /**		The pinmux source mux select value */
	uint32_t drive_strength;                                        /**		The drive strength setting value */
	bool schmitt_trigger_enable;                                    /**		Set to true to configure input pins with Schmitt Trigger */
	bool pullup_pulldown_enablement;                                /**		Set to true if pullup/pulldown enablement is desired */
	bool pullup;                                                    /**		When PullupPulldownEnablement is true, this field
	                                                                 *                              sets the desired pull direction, true denotes pullUp, false=pulldown */
	uint32_t extended_options_1;                                    /** 32-bit field for additional pinmux options/settings, from SMC register x6 */
	uint32_t extended_options_2;                                    /** 32-bit field for additional pinmux options/settings, from SMC register x7 */
} pinctrl_settings;

/**
 *	Pinmux set function, returns true if set command completes successfully, else false
 *		all configuration parameters within the options parameter
 *
 */
bool adi_adrv906x_pinctrl_set(const pinctrl_settings settings, uintptr_t base_addr);

/**
 *	Pinmux set group function, for use by secure_world software
 *		configures groups of I/O defined by the incoming pinctrl_settings array
 *		returns true upon success.
 *
 */
bool adi_adrv906x_pinctrl_set_group(const pinctrl_settings pin_group_settings[], const size_t pin_grp_members, uintptr_t base_addr);

#endif /* __ADI_ADRV906X_PINCTRL_H__ */
