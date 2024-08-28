/*
 * Copyright (c) 2024, Analog Devices Incorporated - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include "drivers/adi/adi_twi_i2c.h"
#include <kernel/delay.h>
#include <io.h>
#include <mm/core_memprot.h>
#include <tee_api_types.h>
#include <trace.h>

#include "adi_twi_i2c_regmap.h"                                 /* I2C (TWI) block registers */

#define I2C_M_WRITE                        0x00                 /* Write to current or specified internal device address */
#define I2C_M_READ                         0x01                 /* Read from current internal device address */
#define I2C_M_READ_COMBO                   0x02                 /* Read from specified internal device address */
#define I2C_M_STOP                         0x04                 /* Manually stop the transmission/reception */

#define MAX_ADDR_BYTES                     2                    /* Supported 1 or 2 byte addresses */
#define TWI_REF_CLOCK                      (10 * 1000 * 1000)   /* 10 MHz */
#define TIMEOUT_US_DELAY                   (50 * 1000)          /* 50 ms (much larger than 1 I2C byte at the slowest speed) */
#define BUS_BUSY_TIMEOUT_US                10                   /* 10 us */

#define ADI_TWI_REG_SIZE                   0x100
#define DEVCLK_FREQ_DFLT                   245760000U


static uint16_t twi_reg_read(vaddr_t addr)
{
	return io_read16(addr);
}

static void twi_reg_write(vaddr_t addr, uint16_t value)
{
	io_write16(addr, value);
}

static uint32_t wait_for_completion(struct adi_i2c_handle *hi2c, uint32_t flags, uint8_t *addr_buf, uint32_t addr_len, uint8_t *data, uint32_t data_len)
{
	uint32_t dcnt;
	uint16_t int_stat;
	uint16_t ctrl;
	uint64_t timeout = timeout_init_us(TIMEOUT_US_DELAY);
	vaddr_t base = hi2c->va;

	/* Format:
	 *
	 * Write:
	 *
	 *                  /------------- optional ------------------\
	 * S | DEV_ADDR | W | ADDR BYTE 1 | A | ... | ADDR_BYTE M | A |.DATA BYTE 1 | A | ... | DATA BYTE N | A | P |
	 *                  :                       :                 :                       :                     :
	 *                  :                       :                 :                       :                     :
	 *                TXSERV                  TXSERV            TXSERV                  TXSERV                MCOMP
	 *
	 * Read:
	 *
	 *   /------------------------ optional --------------------------\
	 * S | DEV_ADDR | W | ADDR BYTE 1 | A | ... | ADDR_BYTE M | A |.S | DEV_ADDR | R | DATA BYTE 1 | A | ... | DATA BYTE N | A | P |
	 *                  :                       :                 :                                :                       :       :
	 *                  :                       :                 :                                :                       :       :
	 *                TXSERV                  TXSERV            MCOMP                            RXSERV                  RXSERV  MCOMP
	 *
	 * where:
	 *   M = 0, 1 or 2
	 *   N >= 0
	 *   TXSERV = FIFO to shift register indication (ready to send data)
	 *   XXSERV = shift register to FIFO indication (received data)
	 *   MCOMP  = transfer completed indication
	 *
	 * Note:
	 * - DEV_ADDR byte is set before calling this function
	 * - The first data byte, if any, to send (ADDR BYTE 1 or DATA BYTE 1)
	 *       was already pushed to the FIFO before calling this function.
	 */

	do {
		int_stat = twi_reg_read(base + TWI_ISTAT);

		if (int_stat & TWI_ISTAT_TXSERV) {
			twi_reg_write(base + TWI_ISTAT, TWI_ISTAT_TXSERV);

			/* Sanity check */
			if (flags & I2C_M_READ) {
				EMSG("Unexpected transmission\n");
				break;
			}

			if (addr_len) {
				twi_reg_write(base + TWI_TXDATA8, *(addr_buf++));
				addr_len--;
			} else if (data_len && !(flags & I2C_M_READ_COMBO)) {
				twi_reg_write(base + TWI_TXDATA8, *(data++));
				data_len--;
			} else {
				ctrl = twi_reg_read(base + TWI_MSTRCTL);
				if (flags & I2C_M_READ_COMBO)
					twi_reg_write(base + TWI_MSTRCTL, ctrl | TWI_MSTRCTL_RSTART | TWI_MSTRCTL_DIR);
				else if (flags & I2C_M_STOP)
					twi_reg_write(base + TWI_MSTRCTL, ctrl | TWI_MSTRCTL_STOP);
			}
		}

		if (int_stat & TWI_ISTAT_RXSERV) {
			twi_reg_write(base + TWI_ISTAT, TWI_ISTAT_RXSERV);

			/* Sanity check */
			if (!(flags & (I2C_M_READ | I2C_M_READ_COMBO))) {
				EMSG("Unexpected reception\n");
				break;
			}

			if (data_len) {
				*(data++) = twi_reg_read(base + TWI_RXDATA8);
				data_len--;
			}

			if ((data_len == 0) && (flags & I2C_M_STOP)) {
				ctrl = twi_reg_read(base + TWI_MSTRCTL);
				twi_reg_write(base + TWI_MSTRCTL, ctrl | TWI_MSTRCTL_STOP);
			}
		}

		if (int_stat & TWI_ISTAT_MERR) {
			twi_reg_write(base + TWI_ISTAT, TWI_ISTAT_MERR);
			return data_len;
		}

		if (int_stat & TWI_ISTAT_MCOMP) {
			twi_reg_write(base + TWI_ISTAT, TWI_ISTAT_MCOMP);

			if (((flags & I2C_M_READ_COMBO)) && data_len) {
				/* Address sent. Start the receive transfer */
				ctrl = twi_reg_read(base + TWI_MSTRCTL);
				if (data_len >= 255) {
					/* Stop signal generated manually */
					dcnt = 0xFF;
					flags |= I2C_M_STOP;
				} else {
					/* Stop signal generated automatically */
					dcnt = data_len;
				}
				ctrl = (ctrl & ~TWI_MSTRCTL_RSTART) | (dcnt << TWI_MSTRCTL_DCNT_OFFSET) | TWI_MSTRCTL_EN | TWI_MSTRCTL_DIR;

				twi_reg_write(base + TWI_MSTRCTL, ctrl);
			} else {
				break;
			}
		}

		if (int_stat)
			timeout = timeout_init_us(TIMEOUT_US_DELAY);
	} while (!timeout_elapsed(timeout));

	return data_len;
}

static TEE_Result adi_twi_i2c_xfer(struct adi_i2c_handle *hi2c, uint32_t flags, uint8_t dev_addr, uint32_t addr, uint32_t addr_len, uint8_t *data, uint32_t data_len)
{
	uint32_t dcnt;
	uint8_t clkhilow;
	uint16_t twi_clk;
	uint16_t value;
	uint8_t addr_buffer[MAX_ADDR_BYTES];
	uint8_t *addr_buf = addr_buffer;
	int ret;
	vaddr_t base = hi2c->va;
	uint64_t timeout;

	/* Sanity checks */
	if ((flags != I2C_M_WRITE) && (flags != I2C_M_READ)) {
		EMSG("Operation not supported\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if (dev_addr > 0x7F) {
		EMSG("TWI only supports 7-bit address mode\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if (addr_len > MAX_ADDR_BYTES) {
		EMSG("Only supported 1 or 2 byte addresses\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if ((data == NULL) && (data_len > 0)) {
		EMSG("Null data pointer\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if (!(twi_reg_read(base + TWI_CTL) & TWI_CTL_EN)) {
		EMSG("TWI is disabled\n");
		return TEE_ERROR_BAD_STATE;
	}

	timeout = timeout_init_us(BUS_BUSY_TIMEOUT_US);
	while (twi_reg_read(base + TWI_MSTRSTAT) & TWI_MSTRSTAT_BUSBUSY)
		if (timeout_elapsed(timeout)) {
			EMSG("TWI line is busy\n");
			return TEE_ERROR_BUSY;
		}

	/* Convert addr to addr_buf (MSB order). Example (addr_len=2): 0x1234 -> 0x34,0x12 */
	for (uint32_t i = 0; i < addr_len; i++)
		addr_buf[i] = (addr >> (8 * (addr_len - 1 - i))) & 0xff;

	/* Mark read combined operation */
	if ((flags & I2C_M_READ) && addr_len) {
		flags = I2C_M_READ_COMBO;
		dcnt = addr_len;
	} else {
		dcnt = addr_len + data_len;
	}

	if (dcnt >= 255) {
		/* For transfer sizes equal or larger than 255 bytes, disable
		 * the internal counter (0xFF) and assert the STOP signal
		 * manually (I2C_M_STOP)
		 */
		dcnt = 0xFF;
		flags |= I2C_M_STOP;
	}

	/* Discard data in FIFOs before starting a new transfer */
	twi_reg_write(base + TWI_FIFOCTL, TWI_FIFOCTL_TXFLUSH | TWI_FIFOCTL_RXFLUSH);
	twi_reg_write(base + TWI_FIFOCTL, 0);

	/* Set slave device address */
	twi_reg_write(base + TWI_MSTRADDR, dev_addr);

	/* Set the first byte to send (if any) */
	if (addr_len) {
		twi_reg_write(base + TWI_TXDATA8, *(addr_buf++));
		addr_len--;
	} else if (data_len && (flags & I2C_M_READ)) {
		twi_reg_write(base + TWI_TXDATA8, *(data++));
		data_len--;
	}

	/* Clear status bits */
	twi_reg_write(base + TWI_MSTRSTAT, 0xFFFF);
	twi_reg_write(base + TWI_ISTAT, 0xFFFF);

	/* Set data transfer counter */
	twi_reg_write(base + TWI_MSTRCTL, dcnt << TWI_MSTRCTL_DCNT_OFFSET);

	/* Recover twi_clk from current configuration */
	clkhilow = twi_reg_read(base + TWI_CLKDIV);
	twi_clk = (10 * 1000) / ((clkhilow >> 8) + clkhilow - 1);

	/* Start transfer */
	value = twi_reg_read(base + TWI_MSTRCTL) | TWI_MSTRCTL_EN |
		((flags & I2C_M_READ) ? TWI_MSTRCTL_DIR : 0) |
		((twi_clk > 100) ? TWI_MSTRCTL_FAST : 0);
	twi_reg_write(base + TWI_MSTRCTL, value);

	ret = wait_for_completion(hi2c, flags, addr_buf, addr_len, data, data_len);

	if (ret) {
		value = twi_reg_read(base + TWI_MSTRCTL) & ~TWI_MSTRCTL_EN;
		twi_reg_write(base + TWI_MSTRCTL, value);

		value = twi_reg_read(base + TWI_CTL) & ~TWI_CTL_EN;
		twi_reg_write(base + TWI_CTL, value);

		value = twi_reg_read(base + TWI_CTL) | TWI_CTL_EN;
		twi_reg_write(base + TWI_CTL, value);

		return TEE_ERROR_COMMUNICATION;
	}

	return TEE_SUCCESS;
}

TEE_Result adi_twi_i2c_write(struct adi_i2c_handle *hi2c, uint8_t dev_addr, uint32_t addr, uint32_t addr_len, uint8_t *data, uint32_t data_len)
{
	return adi_twi_i2c_xfer(hi2c, I2C_M_WRITE, dev_addr, addr, addr_len, data, data_len);
}

TEE_Result adi_twi_i2c_read(struct adi_i2c_handle *hi2c, uint8_t dev_addr, uint32_t addr, uint32_t addr_len, uint8_t *data, uint32_t data_len)
{
	return adi_twi_i2c_xfer(hi2c, I2C_M_READ, dev_addr, addr, addr_len, data, data_len);
}


TEE_Result adi_twi_i2c_init(struct adi_i2c_handle *hi2c)
{
	uint16_t value;
	uint8_t clkhilow = 0;
	vaddr_t base;

	/* Sanity checks */
	if ((hi2c->twi_clk < I2C_SPEED_MIN) || (hi2c->twi_clk > I2C_SPEED_MAX)) {
		EMSG("TWI clock is (%d KHz) out of range (%d-%d Hz)", hi2c->twi_clk, I2C_SPEED_MIN, I2C_SPEED_MAX);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if (hi2c == NULL) {
		EMSG("i2c_handler is NULL pointer\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	base = (vaddr_t)phys_to_virt_io(hi2c->pa, ADI_TWI_REG_SIZE);
	if (!base) {
		EMSG("Unable to get virtual base address\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Store virtual base address */
	hi2c->va = base;

	/* Disable interrupts */
	twi_reg_write(base + TWI_IMSK, 0);

	/* Set TWI internal time reference (10MHz) */
	value = hi2c->sclk / TWI_REF_CLOCK;
	if ((hi2c->sclk % TWI_REF_CLOCK) != 0) value++; /* Added +1 to ensure that internal reference is <= 10MHz, if it doesn't divide evenly */
	twi_reg_write(base + TWI_CTL, value & 0x7F);

	/* Set TWI interface clock (duty cycle 50%) */
	clkhilow = ((TWI_REF_CLOCK / hi2c->twi_clk) + 1) / 2;
	value = (clkhilow << 8) | clkhilow;
	twi_reg_write(base + TWI_CLKDIV, value);

	/* Enable TWI */
	value = twi_reg_read(base + TWI_CTL) | TWI_CTL_EN;
	twi_reg_write(base + TWI_CTL, value);

	return TEE_SUCCESS;
}
