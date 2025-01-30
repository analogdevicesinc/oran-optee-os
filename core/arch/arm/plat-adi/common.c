// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025 Analog Devices Incorporated
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
#include <libfdt.h>
#include <platform_config.h>
#include <printk.h>
#include <rng_support.h>
#include <runtime_log.h>
#include <stdarg.h>
#include <stdio.h>

#define MAX_NODE_NAME_LENGTH            200
#define MAX_NODE_STRING_LENGTH          200
#define DT_LOG_MESSAGE_MAX              512

static struct gic_data gic_data;
static struct pl011_data console_data;

/* Anti-rollback counters */
static uint32_t anti_rollback_counter = 0;
static uint32_t te_anti_rollback_counter = 0;

/* Read the anti-rollback counter value from the device tree, store locally */
static void init_anti_rollback_counter(void)
{
	void *fdt;
	int offset;
	const fdt32_t *prop;

	anti_rollback_counter = 0;

	fdt = get_external_dt();
	if (fdt != 0) {
		offset = fdt_path_offset(fdt, "/boot/anti-rollback");
		if (offset >= 0) {
			prop = fdt_getprop(fdt, offset, "anti-rollback-counter", NULL);
			if (prop != NULL)
				anti_rollback_counter = fdt32_to_cpu(*prop);
		}
	}
}

/* Read the TE anti-rollback counter value from the device tree, store locally */
static void init_te_anti_rollback_counter(void)
{
	void *fdt;
	int offset;
	const fdt32_t *prop;

	te_anti_rollback_counter = 0;

	fdt = get_external_dt();
	if (fdt != 0) {
		offset = fdt_path_offset(fdt, "/boot/anti-rollback");
		if (offset >= 0) {
			prop = fdt_getprop(fdt, offset, "te-anti-rollback-counter", NULL);
			if (prop != NULL)
				te_anti_rollback_counter = fdt32_to_cpu(*prop);
		}
	}
}

/* Log error message to U-Boot device tree */
static void write_error_log(const char *input)
{
	void *fdt;
	int offset;
	const fdt32_t *prop;
	uint32_t error_num;
	int err = -1;
	char name[MAX_NODE_NAME_LENGTH];

	fdt = get_external_dt();
	if (fdt != 0) {
		/* Get error-log node */
		offset = fdt_path_offset(fdt, "/boot/error-log");
		if (offset >= 0) {
			/* Get number of errors in error-log */
			prop = fdt_getprop(fdt, offset, "errors", NULL);
			if (prop != NULL) {
				error_num = fdt32_to_cpu(*prop);

				/* Get property name for this error */
				snprintf(name, MAX_NODE_NAME_LENGTH, "error-%d", error_num);

				/* Set property with error/warning message */
				err = fdt_setprop_string(fdt, offset, name, input);
				if (err != 0) {
					IMSG("Unable to log error to device tree\n");
					return;
				}

				/* Set new number of errors */
				err = fdt_setprop_u32(fdt, offset, "errors", error_num + 1);
				if (err != 0)
					IMSG("Unable to update log\n");
			}
		}
	}
}

/* Return the cached copy of the anti-rollback value from the device tree */
uint32_t plat_get_anti_rollback_counter(void)
{
	return anti_rollback_counter;
}

/* Return the cached copy of the TE anti-rollback value from the device tree */
uint32_t plat_get_te_anti_rollback_counter(void)
{
	return te_anti_rollback_counter;
}

void common_main_init_gic(void)
{
	vaddr_t gicd_base;

	gicd_base = core_mmu_get_va(GIC_BASE, MEM_AREA_IO_SEC, 1);

	if (!gicd_base)
		panic();

	gic_init_base_addr(&gic_data, 0, gicd_base);
	itr_init(&gic_data.chip);

	init_anti_rollback_counter();
	init_te_anti_rollback_counter();
}

static int get_dt_error_num(void)
{
	void *fdt;
	int offset;
	const fdt32_t *prop;
	int error_num = -1;

	/* Get number of errors in error-log */
	fdt = get_external_dt();
	if (fdt != 0) {
		/* Get error-log node offset */
		offset = fdt_path_offset(fdt, "/boot/error-log");
		if (offset >= 0) {
			prop = fdt_getprop(fdt, offset, "errors", NULL);
			if (prop != NULL)
				error_num = fdt32_to_cpu(*prop);
		}
	}

	return error_num;
}

/* Log message in device tree */
static void plat_log_dt_message(const char *label, char *message)
{
	char log[MAX_NODE_STRING_LENGTH + 6];

	if (get_dt_error_num() >= DT_LOG_MESSAGE_MAX) {
		IMSG("Unable to log message to device tree, maximum exceeded\n");
		return;
	}

	/* Add label to beginning of message */
	memcpy(log, label, strlen(label));
	memcpy(log + strlen(label), message, strlen(message) + 1);

	/* Log to device tree */
	write_error_log(log);
}

void __printf(1, 2) plat_error_message(const char *fmt, ...){
	char message[MAX_NODE_STRING_LENGTH];
	va_list args;

	va_start(args, fmt);
	vsnprintf(message, MAX_NODE_STRING_LENGTH, fmt, args);
	va_end(args);

	plat_log_dt_message("E/TC: ", message);
	EMSG("%s\n", message);
}

void __printf(1, 2) plat_warn_message(const char *fmt, ...){
	char message[MAX_NODE_STRING_LENGTH];
	va_list args;

	va_start(args, fmt);
	vsnprintf(message, MAX_NODE_STRING_LENGTH, fmt, args);
	va_end(args);

	plat_log_dt_message("W/TC: ", message);
	IMSG("WARNING: %s\n", message);
}

/* Log runtime message */
static void plat_log_runtime_message(const char *label, char *message)
{
	char log[MAX_NODE_STRING_LENGTH + 6];

	/* Add label to beginning of message */
	memcpy(log, label, strlen(label));
	memcpy(log + strlen(label), message, strlen(message) + 1);

	/* Log to runtime buffer */
	write_to_runtime_buffer(log);
}

/* Record runtime error message */
void __printf(1, 2) plat_runtime_error_message(const char *fmt, ...){
	char message[MAX_NODE_STRING_LENGTH];
	va_list args;

	va_start(args, fmt);
	vsnprintf(message, MAX_NODE_STRING_LENGTH, fmt, args);
	va_end(args);

	plat_log_runtime_message("E/TC: ", message);
	EMSG("%s\n", message);
}

/* Record runtime warning message */
void __printf(1, 2) plat_runtime_warn_message(const char *fmt, ...){
	char message[MAX_NODE_STRING_LENGTH];
	va_list args;

	va_start(args, fmt);
	vsnprintf(message, MAX_NODE_STRING_LENGTH, fmt, args);
	va_end(args);

	plat_log_runtime_message("W/TC: ", message);
	IMSG("WARNING: %s\n", message);
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
		plat_error_message("Unable to get HUK");
		return TEE_ERROR_GENERIC;
	}
	if (huk_buf_len != HW_UNIQUE_KEY_LENGTH) {
		plat_error_message("HUK size is invalid");
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
		plat_error_message("Unable to get random byte");
		panic();
	}

	return seed;
}
