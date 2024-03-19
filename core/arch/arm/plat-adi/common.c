// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024 Analog Devices Incorporated
 */

#include <console.h>
#include <common.h>
#include <drivers/adi/adi_te_interface.h>
#include <drivers/gic.h>
#include <drivers/pl011.h>
#include <kernel/boot.h>
#include <kernel/interrupt.h>
#include <kernel/panic.h>
#include <kernel/tee_common_otp.h>
#include <platform_config.h>
#include <rng_support.h>

static struct gic_data gic_data;
static struct pl011_data console_data;

void common_main_init_gic(void)
{
	vaddr_t gicd_base;

	gicd_base = core_mmu_get_va(GIC_BASE, MEM_AREA_IO_SEC, 1);

	if (!gicd_base)
		panic();

	gic_init_base_addr(&gic_data, 0, gicd_base);
	itr_init(&gic_data.chip);
}

void itr_core_handler(void)
{
	gic_it_handle(&gic_data);
}

void console_init(void)
{
	/* UART0 console initialized in TF-A. No need to reinitialize with clock and baud rate */
	pl011_init(&console_data, CONSOLE_UART_BASE, 0, 0);
	register_serial_console(&console_data.chip);
}

TEE_Result tee_otp_get_hw_unique_key(struct tee_hw_unique_key *hwkey)
{
	int status = 0;
	uint8_t huk_buf[HW_UNIQUE_KEY_LENGTH] = { 0 };
	uint32_t huk_buf_len = sizeof(huk_buf);

	status = adi_enclave_get_huk(TE_MAILBOX_BASE, huk_buf, &huk_buf_len);
	if (status != 0) {
		EMSG("Unable to get HUK\n");
		return TEE_ERROR_GENERIC;
	}
	if (huk_buf_len != HW_UNIQUE_KEY_LENGTH) {
		EMSG("HUK size is invalid\n");
		return TEE_ERROR_GENERIC;
	}

	memcpy(&hwkey->data[0], huk_buf, sizeof(hwkey->data));

	return TEE_SUCCESS;
}

uint8_t hw_get_random_byte(void)
{
	uint8_t seed;
	int status = 0;

	status = adi_enclave_random_bytes(TE_MAILBOX_BASE, &seed, sizeof(seed));
	if (status != 0) {
		EMSG("Unable to get random byte\n");
		panic();
	}

	return seed;
}
