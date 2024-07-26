/*
 * Copyright (c) 2024, Analog Devices Incorporated - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ADRV906X_OTP_H
#define ADRV906X_OTP_H

#include <stdint.h>

#define MAC_ADDRESS_NUM_BYTES  6        /* Number of bytes in a MAC address */
#define NUM_MAC_ADDRESSES      6        /* Number of different MAC addresses to store */

void adrv906x_otp_init_driver(void);
int adrv906x_otp_get_product_id(const uintptr_t mem_ctrl_base, uint8_t *id);
int adrv906x_otp_get_rollback_counter(const uintptr_t mem_ctrl_base, unsigned int *nv_ctr);
int adrv906x_otp_set_rollback_counter(const uintptr_t mem_ctrl_base, unsigned int nv_ctr);
int adrv906x_otp_set_mac_addr(const uintptr_t mem_ctrl_base, uint8_t mac_number, uint8_t *mac);
int adrv906x_otp_get_mac_addr(const uintptr_t mem_ctrl_base, uint8_t mac_number, uint8_t *mac);

#endif /* ADRV906X_OTP_H */
