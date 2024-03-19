// SPDX-License-Identifier: BSD-2-Clause
/*
 * ADI ADRV906X PINTMUX Driver
 *
 * Copyright (c) 2023, Analog Devices Incorporated. All rights reserved.
 */
#include <stdbool.h>
#include <stddef.h>

#include <kernel/thread.h>

#include <drivers/adi/adrv906x/adi_adrv906x_pintmux.h>

/* SMC SiP Service ID */
#define ADI_PINTMUX_SIP_SERVICE_FUNCTION_ID      0xC2000002

/* Intmux Service Function IDs */
#define ADI_PINCTRL_MAP                 1U
#define ADI_PINCTRL_UNMAP               2U

/* SMC SiP standardized return values (res.a0) */
#define SMC_OK                          0x0000000000000000U
#define SMC_ERR_UNKNOWN_FID             0xFFFFFFFFFFFFFFFFU

/* Intmux Service custom return values (res.a1) */
#define ADI_PINTMUX_ERR_LOOKUP_FAIL      0xFFFFFFFFFFFFFFFFU
#define ADI_PINTMUX_ERR_MAP_FAIL         0xFFFFFFFFFFFFFFFEU
#define ADI_PINTMUX_ERR_NOT_MAPPED       0xFFFFFFFFFFFFFFFDU
#define ADI_PINTMUX_ERR_SECURITY         0xFFFFFFFFFFFFFFFCU

static bool adi_adrv906x_pintmux_smc(unsigned int fid, unsigned int gpio, unsigned int *irq, bool polarity, uintptr_t base_addr)
{
	struct thread_smc_args args;

	args.a0 = ADI_PINTMUX_SIP_SERVICE_FUNCTION_ID;
	args.a1 = fid;
	args.a2 = gpio;
	args.a3 = polarity;
	args.a4 = base_addr;

	thread_smccc(&args);

	if (args.a0 != SMC_OK) {
		EMSG("OPTEE :: %s :: SMC_ERR 0x%016lx\n", __FUNCTION__, args.a0);
		return false;
	}

	switch (args.a1) {
	case ADI_PINTMUX_ERR_LOOKUP_FAIL:
	case ADI_PINTMUX_ERR_MAP_FAIL:
	case ADI_PINTMUX_ERR_NOT_MAPPED:
	case ADI_PINTMUX_ERR_SECURITY:
		EMSG("OPTEE :: %s :: PINTMUX_ERR 0x%016lx\n", __FUNCTION__, args.a1);
		return false;
	default:
		break;
	}

	if (irq)
		*irq = (unsigned int)args.a1;

	return true;
}

bool adi_adrv906x_pintmux_map(unsigned int gpio, unsigned int *irq, bool polarity, uintptr_t base_addr)
{
	if (!irq) {
		/* if you map a GPIO, you'll need to keep track of which IRQ it
		 * got assigned to. if `irq` is not a valid pointer, this map
		 * will get lost! */
		EMSG("OPTEE :: %s :: Invalid arguments\n", __FUNCTION__);
		return false;
	}
	return adi_adrv906x_pintmux_smc(ADI_PINCTRL_MAP, gpio, irq, polarity, base_addr);
}

bool adi_adrv906x_pintmux_unmap(unsigned int gpio, uintptr_t base_addr)
{
	return adi_adrv906x_pintmux_smc(ADI_PINCTRL_UNMAP, gpio, NULL, true, base_addr);
}
