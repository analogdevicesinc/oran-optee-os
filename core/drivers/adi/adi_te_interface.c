// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2023, Analog Devices Incorporated. All rights reserved.
 */

#include <assert.h>
#include <io.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <initcall.h>
#include <kernel/cache_helpers.h>
#include <kernel/delay.h>
#include <mm/core_memprot.h>
#include <tee_api_types.h>

#include <drivers/adi/adi_te_interface.h>

#include "adi_te_mailbox.h"

#define HOST_ERROR_INVALID_ARGS (0x01UL)        /* 0x01 - Host error invalid arguments */
#define HOST_ERROR_BUFFER       (0x02UL)
#define NUM_MAILBOX_DATA_REGS (10)
#define CHALLENGE_SIZE_MAX_BYTES (16)           /* 128-bit nonce */
#define RESPONSE_SIZE_BYTES (2 * 256 / 8)       /* R,S of Ed25519 */
#define KEYC_KEY_SIZE_16 (16u)
#define KEYC_KEY_SIZE_TAG (8u)
#define ADI_TE_RET_OK   (0)
#define TE_RESPONSE_TIMEOUT_US_8_S        (8000000U)
#define ETIMEDOUT       60

#define ADI_TE_MAILBOX_REG_SIZE         (0x1000)

uint32_t mb_regs_mdr[NUM_MAILBOX_DATA_REGS] = {
	MB_REGS_MDR0,
	MB_REGS_MDR1,
	MB_REGS_MDR2,
	MB_REGS_MDR3,
	MB_REGS_MDR4,
	MB_REGS_MDR5,
	MB_REGS_MDR6,
	MB_REGS_MDR7,
	MB_REGS_MDR8,
	MB_REGS_MDR9,
};

typedef enum {
	ADI_ENCLAVE_GET_ENCLAVE_VERSION		= 0x00,
	ADI_ENCLAVE_GET_MAILBOX_VERSION		= 0x01,
	ADI_ENCLAVE_GET_API_VERSION		= 0x02,
	ADI_ENCLAVE_ENABLE_FEATURE		= 0x06,
	ADI_ENCLAVE_GET_ENABLED_FEATURES	= 0x07,
	ADI_ENCLAVE_GET_DEVICE_IDENTITY		= 0x09,
	ADI_ENCLAVE_GET_SERIAL_NUMBER		= 0x0b,
	ADI_ENCLAVE_INCR_ANTIROLLBACK_VERSION	= 0x1c,
	ADI_ENCLAVE_GET_ANTIROLLBACK_VERSION	= 0x1d,
	ADI_ENCLAVE_GET_HUK			= 0x1e,
	ADI_ENCLAVE_REQUEST_CHALLENGE		= 0x80,
	ADI_ENCLAVE_PRIV_SECURE_DEBUG_ACCESS	= 0x8a,
	ADI_ENCLAVE_PRIV_SET_RMA		= 0x8b,
	ADI_ENCLAVE_PROV_HSTKEY			= 0x90,
	ADI_ENCLAVE_PROV_PREPARE_FINALIZE	= 0x91,
	ADI_ENCLAVE_PROV_FINALIZE		= 0x92,
	ADI_ENCLAVE_UNWRAP_CUST_KEY		= 0x182,
	ADI_ENCLAVE_WRAP_CUST_KEY		= 0x183,
	ADI_ENCLAVE_RANDOM			= 0x184,
} adi_enclave_api_id_t;

static uint8_t te_buf[1024] __attribute__((aligned(sizeof(uint64_t))));    /* Buffer to transfer data through TE mailbox */
static uintptr_t cur_ptr = (uintptr_t)te_buf;

adi_lifecycle_t adi_enclave_get_lifecycle_state(uintptr_t base_addr)
{
	vaddr_t va = (vaddr_t)phys_to_virt_io(base_addr, ADI_TE_MAILBOX_REG_SIZE);

	return (io_read32(va + MB_REGS_LIFECYCLE_STATUS) & MB_REGS_LIFECYCLE_ENCODE_MASK) >> MB_REGS_LIFECYCLE_ENCODE_BITP;
}

static void ack_response(uintptr_t base_addr)
{
	io_write32(base_addr + MB_REGS_H_STATUS, MB_REGS_ERESP_ACK);
}

static void signal_request_ready(uintptr_t base_addr)
{
	io_write32(base_addr + MB_REGS_H_STATUS, MB_REGS_HREQ_RDY);
}

static int wait_for_response(uintptr_t base_addr)
{
	uint32_t status = 0;
	uint64_t timeout;

	timeout = timeout_init_us(TE_RESPONSE_TIMEOUT_US_8_S);
	while ((status & MB_REGS_ERESP_RDY) != MB_REGS_ERESP_RDY) {
		if (timeout_elapsed(timeout))
			return -ETIMEDOUT;
		status = io_read32(base_addr + MB_REGS_E_STATUS);
	}

	return ADI_TE_RET_OK;
}

static int verify_buf_len_ptr(const void *buf, uint32_t *buflen, uint32_t minlen, uint32_t maxlen)
{
	if (buf == NULL || buflen == NULL)
		return HOST_ERROR_INVALID_ARGS;

	if (minlen <= maxlen && (*buflen < minlen || *buflen > maxlen))
		return HOST_ERROR_INVALID_ARGS;

	/* Check for overflow of te_buf */
	if ((cur_ptr + *buflen) > ((uintptr_t)te_buf + sizeof(te_buf)))
		return HOST_ERROR_BUFFER;

	return ADI_TE_RET_OK;
}

static int verify_buf_len(const void *buf, uint32_t buflen, uint32_t minlen, uint32_t maxlen)
{
	return verify_buf_len_ptr(buf, &buflen, minlen, maxlen);
}

/* Copy buffer to te_buf which is used to transfer data through the mailbox */
static uintptr_t reserve_buf(uintptr_t buf, uint32_t size)
{
	uintptr_t old_ptr = cur_ptr;

	memcpy((void *)cur_ptr, (void *)buf, size);

	cur_ptr += size;

	return old_ptr;
}

/* Set pointer back to start of te_buf and clear buffer */
static void buf_init(void)
{
	cur_ptr = (uintptr_t)te_buf;

	memset(te_buf, 0, sizeof(te_buf));
}

/* Data sent through TE mailbox must be copied to te_buf prior to calling this function to be able to flush/invalidate memory */
static int perform_enclave_transaction(uintptr_t base_addr, adi_enclave_api_id_t requestId, uint32_t args[], uint32_t numArgs)
{
	uint32_t i;
	int ret;
	vaddr_t va;

	if ((args == NULL && numArgs != 0) || (numArgs > NUM_MAILBOX_DATA_REGS))
		return HOST_ERROR_INVALID_ARGS;

	/* Clean cache */
	dcache_clean_range((void *)virt_to_phys((void *)te_buf), sizeof(te_buf));

	va = (vaddr_t)phys_to_virt_io(base_addr, ADI_TE_MAILBOX_REG_SIZE);

	io_write32(va + MB_REGS_HRC0, requestId);

	for (i = 0; i < numArgs; i++)
		io_write32(va + mb_regs_mdr[i], args[i]);

	signal_request_ready(va);

	ret = wait_for_response(va);
	if (ret != ADI_TE_RET_OK) {
		EMSG("Timed out waiting for Enclave mailbox response\n");
		return ret;
	}

	ack_response(va);

	/* Invalidate cache */
	dcache_inv_range((void *)virt_to_phys((void *)te_buf), sizeof(te_buf));

	for (i = 0; i < numArgs; i++)
		args[i] = io_read32(va + mb_regs_mdr[i]);

	return io_read32(va + MB_REGS_ERC1);
}

/* Tiny Enclave version */
int adi_enclave_get_enclave_version(uintptr_t base_addr, uint8_t *output_buffer, uint32_t *o_buff_len)
{
	int ret, status;
	uint32_t args[2];

	buf_init();

	ret = verify_buf_len_ptr(output_buffer, o_buff_len, 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[0] = (uint32_t)reserve_buf((uintptr_t)output_buffer, *o_buff_len);

	ret = verify_buf_len(o_buff_len, sizeof(*o_buff_len), 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[1] = (uint32_t)reserve_buf((uintptr_t)o_buff_len, sizeof(*o_buff_len));

	status = perform_enclave_transaction(base_addr, ADI_ENCLAVE_GET_ENCLAVE_VERSION, args, 2);
	if (status == 0) {
		memcpy(output_buffer, (void *)(uintptr_t)args[0], *o_buff_len);
		memcpy(o_buff_len, (void *)(uintptr_t)args[1], sizeof(*o_buff_len));
	}

	return status;
}

/* The version of the mailbox HW block as provided in RTL and memory mapped */
int adi_enclave_get_mailbox_version(uintptr_t base_addr)
{
	return perform_enclave_transaction(base_addr, ADI_ENCLAVE_GET_MAILBOX_VERSION, NULL, 0);
}

/* Get the device serial number provisioned in OTP */
int adi_enclave_get_serial_number(uintptr_t base_addr, uint8_t *output_buffer, uint32_t *o_buff_len)
{
	int ret, status;
	uint32_t args[2];

	buf_init();

	ret = verify_buf_len_ptr(output_buffer, o_buff_len, 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[0] = (uint32_t)reserve_buf((uintptr_t)output_buffer, *o_buff_len);

	ret = verify_buf_len(o_buff_len, sizeof(*o_buff_len), 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[1] = (uint32_t)reserve_buf((uintptr_t)o_buff_len, sizeof(*o_buff_len));

	status = perform_enclave_transaction(base_addr, ADI_ENCLAVE_GET_SERIAL_NUMBER, args, 2);
	if (status == 0) {
		memcpy(output_buffer, (void *)(uintptr_t)args[0], *o_buff_len);
		memcpy(o_buff_len, (void *)(uintptr_t)args[1], sizeof(*o_buff_len));
	}

	return status;
}

/* Only responds while in CUST1_PROV_HOST lifecycle
 * Sets the device lifecycle to DEPLOYED
 */
int adi_enclave_provision_finalize(uintptr_t base_addr)
{
	return perform_enclave_transaction(base_addr, ADI_ENCLAVE_PROV_FINALIZE, NULL, 0);
}

/* Initiate the challenge-response protocol by asking the enclave for the challenge */
int adi_enclave_request_challenge(uintptr_t base_addr, chal_type_e chal_type, uint8_t *output_buffer, uint32_t *o_buff_len)
{
	int ret, status;
	uint32_t args[3];

	buf_init();

	ret = verify_buf_len_ptr(output_buffer, o_buff_len, 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[0] = chal_type;
	args[1] = (uint32_t)reserve_buf((uintptr_t)output_buffer, *o_buff_len);

	ret = verify_buf_len(o_buff_len, sizeof(*o_buff_len), 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[2] = (uint32_t)reserve_buf((uintptr_t)o_buff_len, sizeof(*o_buff_len));

	status = perform_enclave_transaction(base_addr, ADI_ENCLAVE_REQUEST_CHALLENGE, args, 3);
	if (status == 0) {
		memcpy(output_buffer, (void *)(uintptr_t)args[1], *o_buff_len);
		memcpy(o_buff_len, (void *)(uintptr_t)args[2], sizeof(*o_buff_len));
	}

	return status;
}

/* Sets the lifecycle of the part to CUST or ADI RMA depending on the type of RMA challenge requested
 * Calling this API without first calling adi_enclave_request_challenge will result in an error.
 */
int adi_enclave_priv_set_rma(uintptr_t base_addr, const uint8_t *cr_input_buffer, uint32_t input_buff_len)
{
	int ret;
	uint32_t args[2];

	buf_init();

	ret = verify_buf_len(cr_input_buffer, input_buff_len, 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[0] = (uint32_t)reserve_buf((uintptr_t)cr_input_buffer, input_buff_len);
	args[1] = input_buff_len;

	return perform_enclave_transaction(base_addr, ADI_ENCLAVE_PRIV_SET_RMA, args, 2);
}

int adi_enclave_priv_secure_debug_access(uintptr_t base_addr, const uint8_t *cr_input_buffer, uint32_t input_buff_len)
{
	int ret;
	uint32_t args[2];

	buf_init();

	ret = verify_buf_len(cr_input_buffer, input_buff_len, 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[0] = (uint32_t)reserve_buf((uintptr_t)cr_input_buffer, input_buff_len);
	args[1] = input_buff_len;

	return perform_enclave_transaction(base_addr, ADI_ENCLAVE_PRIV_SECURE_DEBUG_ACCESS, args, 2);
}

int adi_enclave_get_api_version(uintptr_t base_addr, uint8_t *output_buffer, uint32_t *o_buff_len)
{
	int ret, status;
	uint32_t args[2];

	buf_init();

	ret = verify_buf_len_ptr(output_buffer, o_buff_len, 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[0] = (uint32_t)reserve_buf((uintptr_t)output_buffer, *o_buff_len);

	ret = verify_buf_len(o_buff_len, sizeof(*o_buff_len), 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[1] = (uint32_t)reserve_buf((uintptr_t)o_buff_len, sizeof(*o_buff_len));

	status = perform_enclave_transaction(base_addr, ADI_ENCLAVE_GET_API_VERSION, args, 2);
	if (status == 0) {
		memcpy(output_buffer, (void *)(uintptr_t)args[0], *o_buff_len);
		memcpy(o_buff_len, (void *)(uintptr_t)args[1], sizeof(*o_buff_len));
	}

	return status;
}

/* Request the enclave to enable a feature/features of the system by issuing a Feature Certificate (FCER) */
int adi_enclave_enable_feature(uintptr_t base_addr, const uint8_t *input_buffer_fcer, uint32_t fcer_len)
{
	int ret;
	uint32_t args[2];

	buf_init();

	ret = verify_buf_len(input_buffer_fcer, fcer_len, 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[0] = (uint32_t)reserve_buf((uintptr_t)input_buffer_fcer, fcer_len);
	args[1] = fcer_len;

	return perform_enclave_transaction(base_addr, ADI_ENCLAVE_ENABLE_FEATURE, args, 2);
}

/* Get what's currently enabled in the system */
int adi_enclave_get_enabled_features(uintptr_t base_addr, uint8_t *output_buffer, uint32_t *o_buff_len)
{
	int ret, status;
	uint32_t args[2];

	buf_init();

	ret = verify_buf_len_ptr(output_buffer, o_buff_len, 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[0] = (uint32_t)reserve_buf((uintptr_t)output_buffer, *o_buff_len);

	ret = verify_buf_len(o_buff_len, sizeof(*o_buff_len), 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[1] = (uint32_t)reserve_buf((uintptr_t)o_buff_len, sizeof(*o_buff_len));

	status = perform_enclave_transaction(base_addr, ADI_ENCLAVE_GET_ENABLED_FEATURES, args, 2);
	if (status == 0) {
		memcpy(output_buffer, (void *)(uintptr_t)args[0], *o_buff_len);
		memcpy(o_buff_len, (void *)(uintptr_t)args[1], sizeof(*o_buff_len));
	}

	return status;
}

int adi_enclave_get_device_identity(uintptr_t base_addr, uint8_t *output_buffer, uint32_t *o_buff_len)
{
	int ret, status;
	uint32_t args[2];

	buf_init();

	ret = verify_buf_len_ptr(output_buffer, o_buff_len, 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[0] = (uint32_t)reserve_buf((uintptr_t)output_buffer, *o_buff_len);

	ret = verify_buf_len(o_buff_len, sizeof(*o_buff_len), 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[1] = (uint32_t)reserve_buf((uintptr_t)o_buff_len, sizeof(*o_buff_len));

	status = perform_enclave_transaction(base_addr, ADI_ENCLAVE_GET_DEVICE_IDENTITY, args, 2);
	if (status == 0) {
		memcpy(output_buffer, (void *)(uintptr_t)args[0], *o_buff_len);
		memcpy(o_buff_len, (void *)(uintptr_t)args[1], sizeof(*o_buff_len));
	}

	return status;
}

/* Only responds while in ADI_PROV_ENC lifecycle and must be called prior to adi_enclave_provisionPrepareFinalize() */
int adi_enclave_provision_host_keys(uintptr_t base_addr, const uintptr_t hst_keys, uint32_t hst_keys_len, uint32_t hst_keys_size)
{
	int ret;
	uint32_t key_num;
	uint32_t args[2];
	host_keys_t *tmp_hst_keys;

	buf_init();

	ret = verify_buf_len((const void *)hst_keys, hst_keys_size, 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	tmp_hst_keys = (host_keys_t *)reserve_buf((uintptr_t)hst_keys, hst_keys_size);

	for (key_num = 0; key_num < hst_keys_len; key_num++) {
		ret = verify_buf_len(tmp_hst_keys[key_num].key, tmp_hst_keys[key_num].key_len, 1, (uint32_t)SIZE_MAX);
		if (ret != ADI_TE_RET_OK)
			return ret;

		tmp_hst_keys[key_num].key = (uint8_t *)reserve_buf((uintptr_t)tmp_hst_keys[key_num].key, tmp_hst_keys[key_num].key_len);
	}

	args[0] = (uintptr_t)tmp_hst_keys;
	args[1] = hst_keys_len;

	return perform_enclave_transaction(base_addr, ADI_ENCLAVE_PROV_HSTKEY, args, 2);
}

/* Only responds while in ADI_PROV_ENC lifecycle
 * This mailbox API will partially complete the remaining items that could not occur during the ADI
 * provisioning in PRFW execution (calculate CRC, set lockout bits, sets device lifecycle to CUST_PROVISIONED)
 * The API adi_enclave_provisionFinalize() is supposed to be called subsequent to this function
 */
int adi_enclave_provision_prepare_finalize(uintptr_t base_addr)
{
	return perform_enclave_transaction(base_addr, ADI_ENCLAVE_PROV_PREPARE_FINALIZE, NULL, 0);
}

/* Use host IPK (c1) in OTP (wrapped by RIPK) to unwrap host c2 key */
int adi_enclave_unwrap_cust_key(uintptr_t base_addr, const void *wrapped_key, uint32_t wk_len,
				void *unwrapped_key, uint32_t *uwk_len)
{
	int ret, status;
	uint32_t args[4];

	buf_init();

	ret = verify_buf_len(wrapped_key, wk_len, KEYC_KEY_SIZE_16 + KEYC_KEY_SIZE_TAG, KEYC_KEY_SIZE_16 + KEYC_KEY_SIZE_TAG);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[0] = (uint32_t)reserve_buf((uintptr_t)wrapped_key, wk_len);
	args[1] = wk_len;

	ret = verify_buf_len(unwrapped_key, *uwk_len, KEYC_KEY_SIZE_16, KEYC_KEY_SIZE_16);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[2] = (uint32_t)reserve_buf((uintptr_t)unwrapped_key, *uwk_len);

	ret = verify_buf_len(uwk_len, *uwk_len, KEYC_KEY_SIZE_16, KEYC_KEY_SIZE_16);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[3] = (uint32_t)reserve_buf((uintptr_t)uwk_len, sizeof(*uwk_len));

	status = perform_enclave_transaction(base_addr, ADI_ENCLAVE_UNWRAP_CUST_KEY, args, 4);
	if (status == 0) {
		memcpy(unwrapped_key, (void *)(uintptr_t)args[2], *uwk_len);
		memcpy(uwk_len, (void *)(uintptr_t)args[3], sizeof(*uwk_len));
	}

	return status;
}

/* Increment the Security version of APP in OTP by 1 on every successive call */
int adi_enclave_update_otp_app_anti_rollback(uintptr_t base_addr, uint32_t *appSecVer)
{
	int ret, status;
	uint32_t args[1];

	buf_init();

	ret = verify_buf_len(appSecVer, sizeof(*appSecVer), 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[0] = (uint32_t)reserve_buf((uintptr_t)appSecVer, sizeof(*appSecVer));

	status = perform_enclave_transaction(base_addr, ADI_ENCLAVE_INCR_ANTIROLLBACK_VERSION, args, 1);
	if (status == 0)
		memcpy(appSecVer, (void *)(uintptr_t)args[0], sizeof(*appSecVer));

	return status;
}

/* Get the Security version of APP in OTP */
int adi_enclave_get_otp_app_anti_rollback(uintptr_t base_addr, uint32_t *appSecVer)
{
	int ret, status;
	uint32_t args[1];

	buf_init();

	ret = verify_buf_len(appSecVer, sizeof(*appSecVer), 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[0] = (uint32_t)reserve_buf((uintptr_t)appSecVer, sizeof(*appSecVer));

	status = perform_enclave_transaction(base_addr, ADI_ENCLAVE_GET_ANTIROLLBACK_VERSION, args, 1);
	if (status == 0)
		memcpy(appSecVer, (void *)(uintptr_t)args[0], sizeof(*appSecVer));

	return status;
}

int adi_enclave_get_huk(uintptr_t base_addr, uint8_t *output_buffer, uint32_t *o_buff_len)
{
	int ret, status;
	uint32_t args[2];

	buf_init();

	ret = verify_buf_len_ptr(output_buffer, o_buff_len, 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[0] = (uint32_t)reserve_buf((uintptr_t)output_buffer, *o_buff_len);

	ret = verify_buf_len(o_buff_len, sizeof(*o_buff_len), 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[1] = (uint32_t)reserve_buf((uintptr_t)o_buff_len, sizeof(*o_buff_len));

	status = perform_enclave_transaction(base_addr, ADI_ENCLAVE_GET_HUK, args, 2);
	if (status == 0) {
		memcpy(output_buffer, (void *)(uintptr_t)args[0], *o_buff_len);
		memcpy(o_buff_len, (void *)(uintptr_t)args[1], sizeof(*o_buff_len));
	}

	return status;
}

int adi_enclave_random_bytes(uintptr_t base_addr, void *output_buffer, uint32_t o_buff_len)
{
	uintptr_t buf = (uintptr_t)output_buffer;
	uint32_t args[2];
	int ret, status;

	buf_init();

	ret = verify_buf_len((const void *)buf, o_buff_len, 1, (uint32_t)SIZE_MAX);
	if (ret != ADI_TE_RET_OK)
		return ret;

	args[0] = (uint32_t)reserve_buf(buf, o_buff_len);
	args[1] = (uint32_t)o_buff_len;

	status = perform_enclave_transaction(base_addr, ADI_ENCLAVE_RANDOM, args, 2);
	if (status == 0)
		memcpy(output_buffer, (void *)(uintptr_t)args[0], o_buff_len);

	return status;
}

bool adi_enclave_is_host_boot_ready(uintptr_t base_addr)
{
	uint32_t reg;

	reg = io_read32(base_addr + MB_REGS_BOOT_FLOW0);

	/* TE is ready for host boot when its boot status indicates LOAD_AND_UNWRAP_KEYS has been performed */
	if ((reg & MB_REGS_BOOT_FLOW0_LOAD_AND_UNWRAP_KEYS_BITM) == MB_REGS_BOOT_FLOW0_LOAD_AND_UNWRAP_KEYS_BITM)
		return true;
	return false;
}
