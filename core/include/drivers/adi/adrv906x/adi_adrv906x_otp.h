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

/* Temp sensor group ids in OTP */
typedef enum {
	TEMP_SENSOR_CLK_ETH_PLL		= 0,
	TEMP_SENSOR_RF0_1_PLL		= 1,
	TEMP_SENSOR_TX0_1		= 2,
	TEMP_SENSOR_TX2_3		= 3,
	TEMP_SENSOR_PLL_SLOPE		= 4,
	TEMP_SENSOR_TX_SLOPE		= 5,
	TEMP_SENSOR_OTP_SLOT_NUM	= 6
} adrv906x_temp_group_id_t;

void adrv906x_otp_init_driver(void);
int adrv906x_otp_get_product_id(const uintptr_t mem_ctrl_base, uint8_t *id);
int adrv906x_otp_get_rollback_counter(const uintptr_t mem_ctrl_base, unsigned int *nv_ctr);
int adrv906x_otp_set_rollback_counter(const uintptr_t mem_ctrl_base, unsigned int nv_ctr);
int adrv906x_otp_set_mac_addr(const uintptr_t mem_ctrl_base, uint8_t mac_number, uint8_t *mac);
int adrv906x_otp_get_mac_addr(const uintptr_t mem_ctrl_base, uint8_t mac_number, uint8_t *mac);
int adrv906x_otp_get_temp_sensor(const uintptr_t mem_ctrl_base, adrv906x_temp_group_id_t temp_group_id, uint32_t *value);

#endif /* ADRV906X_OTP_H */
