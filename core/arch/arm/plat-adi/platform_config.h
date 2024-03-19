/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2022 Analog Devices Incorporated
 */

#ifndef PLATFORM_CONFIG_H
#define PLATFORM_CONFIG_H

#include <mm/generic_ram_layout.h>

/* Make stacks aligned to data cache line length */
#define STACK_ALIGNMENT             64

/* Include SoC-specific configuration */
#ifdef CFG_ADI_ADRV906X_ARCH
#include <adrv906x_config.h>
#endif

#endif /* PLATFORM_CONFIG_H */
