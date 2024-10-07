/*
 * Copyright (c) 2024, Analog Devices Incorporated - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __ADI_TWI_I2C_H__
#define __ADI_TWI_I2C_H__

#include <mm/core_memprot.h>

#define I2C_SPEED_MAX                      (400 * 1000)         /* 400 KHz */
#define I2C_SPEED_MIN                      (21 * 1000)          /* 21 KHz */

struct adi_i2c_handle {
	paddr_t pa;             /* Physical base address */
	vaddr_t va;             /* Virtual base address */
	uint32_t sclk;          /* TWI source clock (Hz) */
	uint32_t twi_clk;       /* TWI interface clock (KHz) */
};

TEE_Result adi_twi_i2c_write(struct adi_i2c_handle *h2ic, uint8_t dev_addr, uint32_t addr, uint32_t addr_len, uint8_t *data, uint32_t data_len);
TEE_Result adi_twi_i2c_read(struct adi_i2c_handle *h2ic, uint8_t dev_addr, uint32_t addr, uint32_t addr_len, uint8_t *data, uint32_t data_len);
TEE_Result adi_twi_i2c_write_read(struct adi_i2c_handle *hi2c, uint8_t dev_addr, uint32_t addr, uint32_t addr_len, uint8_t *data, uint32_t write_data_len, uint32_t read_data_len);
TEE_Result adi_twi_i2c_init(struct adi_i2c_handle *h2ic);

#endif /* __ADI_TWI_I2C_H__ */
