// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025 Analog Devices Incorporated
 */

#include <console.h>
#include <kernel/thread.h>
#include <stdio.h>
#include <string.h>

#include <common.h>
#include <runtime_log.h>

/* PINCTRL Function ID*/
#define ADI_RUNTIME_LOG_SIP_SERVICE_FUNCTION_ID                     0xC2000003

/* SMC Handler return Status Values (res.a0 return value) */
#define ADI_RUNTIME_LOG_SMC_RETURN_SUCCESS                  (0U)
#define ADI_RUNTIME_LOG_SMC_RETURN_UNSUPPORTED_REQUEST      (0xFFFFFFFFFFFFFFFFU)

#define GROUP_SEPARATOR '\x1D'  /* ASCII Group Separator */

static int buffer_length = 0;
static int read_index = 0;
static char runtime_log[SIZE_OF_OPTEE_RUNTIME_BUFFER] = { 0 };
static int write_index = 0;
static bool lock = false;

/* Returns true if buffer is full */
static bool is_buffer_full(void)
{
	if (buffer_length == SIZE_OF_OPTEE_RUNTIME_BUFFER) {
		IMSG("OP-TEE buffer is full\n");
		return true;
	}
	return false;
}

/* Writes one character to runtime buffer */
static void write_char_to_buffer(char write)
{
	/* If buffer is full, cannot write to it */
	if (is_buffer_full())
		return;

	/* Write character to buffer, increment buffer length and write index */
	runtime_log[write_index] = write;
	buffer_length++;
	write_index++;

	/* Circle write index back to beginning of buffer, if past the end */
	if (write_index == SIZE_OF_OPTEE_RUNTIME_BUFFER)
		write_index = 0;
}

/* Read one character from runtime buffer */
static char read_char_from_buffer(void)
{
	char read;

	/* Read one character from buffer, increment read index, and decrement buffer length */
	read = runtime_log[read_index];
	read_index++;
	buffer_length--;

	/* Circle read index back to beginning of buffer, if past the end */
	if (read_index == SIZE_OF_OPTEE_RUNTIME_BUFFER)
		read_index = 0;
	return read;
}

/* Write message to runtime buffer */
void write_to_runtime_buffer(const char *message)
{
	char *tmp = (char *)message;

	/* Obtain lock when available */
	while (lock)
		;
	lock = true;

	/* Write each character to runtime buffer */
	while (*tmp != '\0') {
		write_char_to_buffer(*tmp);
		tmp++;
	}

	/* Write termination character to buffer */
	write_char_to_buffer(GROUP_SEPARATOR);

	/* Unlock */
	lock = false;
}

/* Read messages from runtime buffer */
void read_from_runtime_buffer(char *message, int size)
{
	int index = 0;

	/* Obtain lock when available */
	while (lock)
		;
	lock = true;

	/* Read each character into buffer */
	while (read_index != write_index) {
		if (index >= size)
			break;
		message[index] = read_char_from_buffer();
		index++;
	}

	/* Clear buffer */
	memset(runtime_log, 0, sizeof(runtime_log));
	read_index = write_index;
	buffer_length = 0;

	/* Unlock */
	lock = false;
}

bool adi_runtime_log_smc(char *buffer, int size)
{
	struct thread_smc_args args;

	/*
	 * Setup  smc call to obtain the BL31 runtime log
	 *
	 * thread_smc_args expected params:
	 *    a0: SMC SIP SERVICE ID
	 *    a1: Pointer to buffer
	 *    a2: Size of buffer
	 *    a3: Currently UNUSED/UNDEFINED
	 *    a4: Currently UNUSED/UNDEFINED
	 *    a5: Currently UNUSED/UNDEFINED
	 *    a6: Currently UNUSED/UNDEFINED
	 *    a7: Currently UNUSED/UNDEFINED
	 *
	 * Return Params
	 *    a0: SMC return value
	 *    a1: Currently UNUSED/UNDEFINED
	 */

	args.a0 = ADI_RUNTIME_LOG_SIP_SERVICE_FUNCTION_ID;
	args.a1 = (uintptr_t)buffer;
	args.a2 = size;

	thread_smccc(&args);

	if (args.a0 != ADI_RUNTIME_LOG_SMC_RETURN_SUCCESS) {
		EMSG("OPTEE :: %s :: SMC_ERR 0x%016lx\n", __FUNCTION__, args.a0);
		return false;
	}

	return true;
}
