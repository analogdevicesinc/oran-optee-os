/*
 * Copyright (c) 2023, Analog Devices Incorporated, All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __ADI_ADRV906X_PINTMUX_H__
#define __ADI_ADRV906X_PINTMUX_H__

#include <stdbool.h>

bool adi_adrv906x_pintmux_map(unsigned int gpio, unsigned int *irq, bool polarity, uintptr_t base_addr);
bool adi_adrv906x_pintmux_unmap(unsigned int gpio, uintptr_t base_addr);

#endif /* __ADI_ADRV906X_PINTMUX_H__ */
