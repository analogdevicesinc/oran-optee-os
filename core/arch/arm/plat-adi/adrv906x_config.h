/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2024 Analog Devices Incorporated
 */

#ifndef ADRV906X_CONFIG_H
#define ADRV906X_CONFIG_H

#include <adrv906x_def.h>
#include <adrv906x_reg_offsets.h>

#define CONSOLE_UART_BASE PL011_0_BASE

#define ADI_ADRV906X_PERIPHERAL_BASE                    U(0x20000000)
#define ADI_ADRV906X_PERIPHERAL_SIZE                    U(0x04000000)

#define ADI_ADRV906X_SEC_PERIPHERAL_BASE U(0x24000000)
#define ADI_ADRV906X_SEC_PERIPHERAL_SIZE U(0x04000000)

#endif /* ADRV906X_CONFIG_H */
