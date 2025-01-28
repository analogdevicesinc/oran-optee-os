// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025, Analog Devices Incorporated. All rights reserved.
 */

#include <io.h>
#include <kernel/pseudo_ta.h>
#include <mm/core_memprot.h>
#include <string.h>

#include <adrv906x_util.h>
#include <common.h>
#include <drivers/adi/adi_te_interface.h>

#define TA_NAME         "te_mailbox.ta"

#define TE_MAILBOX_PTA_UUID \
	{ \
		0x47274ef4, \
		0xadfa, 0x4c4b, \
		{ \
			0xa0, 0x0e, \
			0x99, 0x40, \
			0xd2, 0x93, \
			0x76, 0x94, \
		} \
	}

/* Commands */
#define PROV_HOST_KEY_CMD            0
#define PROV_PREP_FINALIZE_CMD            1
#define PROV_FINALIZE_CMD            2
#define BOOT_FLOW_REG_READ            3

/* Parameters for BOOT_FLOW_REG_READ */
#define OP_PARAM_BOOT_FLOW              0

/*
 * adi_te_mailbox_check_params - verify the received parameters are of the expected types
 */
static TEE_Result adi_te_mailbox_check_params(uint32_t param_types, uint32_t cmd)
{
	switch (cmd) {
	case PROV_HOST_KEY_CMD:
		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT, TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE)) {
			plat_runtime_error_message("Bad parameters to provision host key command");
			return TEE_ERROR_BAD_PARAMETERS;
		} else {
			return TEE_SUCCESS;
		}
	case PROV_PREP_FINALIZE_CMD:
		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE)) {
			plat_runtime_error_message("Bad parameters to provision prepare finalize command");
			return TEE_ERROR_BAD_PARAMETERS;
		} else {
			return TEE_SUCCESS;
		}
	case PROV_FINALIZE_CMD:
		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE)) {
			plat_runtime_error_message("Bad parameters to provision finalize command");
			return TEE_ERROR_BAD_PARAMETERS;
		} else {
			return TEE_SUCCESS;
		}
	case BOOT_FLOW_REG_READ:
		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE)) {
			plat_runtime_error_message("Bad parameters to boot flow reg read");
			return TEE_ERROR_BAD_PARAMETERS;
		} else {
			return TEE_SUCCESS;
		}
	default:
		plat_runtime_error_message("Invalid command");
		break;
	}
	return TEE_SUCCESS;
}

/*
 * adi_te_mailbox_check_lifecycle_state - verify lifecycle states for each command
 */
static TEE_Result adi_te_mailbox_check_lifecycle_state(uint32_t cmd)
{
	uint32_t lifecycle = adi_enclave_get_lifecycle_state(TE_MAILBOX_BASE);

	switch (cmd) {
	case PROV_HOST_KEY_CMD:
		if (lifecycle != ADI_LIFECYCLE_ADI_PROV_ENC)
			return TEE_ERROR_GENERIC;
		if (plat_is_dual_tile()) {
			lifecycle = adi_enclave_get_lifecycle_state(SEC_TE_MAILBOX_BASE);
			if (lifecycle != ADI_LIFECYCLE_ADI_PROV_ENC)
				return TEE_ERROR_GENERIC;
		}
		return TEE_SUCCESS;
	case PROV_PREP_FINALIZE_CMD:
		if (lifecycle != ADI_LIFECYCLE_ADI_PROV_ENC)
			return TEE_ERROR_GENERIC;
		if (plat_is_dual_tile()) {
			lifecycle = adi_enclave_get_lifecycle_state(SEC_TE_MAILBOX_BASE);
			if (lifecycle != ADI_LIFECYCLE_ADI_PROV_ENC)
				return TEE_ERROR_GENERIC;
		}
		return TEE_SUCCESS;
	case PROV_FINALIZE_CMD:
		if (lifecycle != ADI_LIFECYCLE_ADI_PROV_ENC && lifecycle != ADI_LIFECYCLE_CUST1_PROV_HOST)
			return TEE_ERROR_GENERIC;
		if (plat_is_dual_tile()) {
			lifecycle = adi_enclave_get_lifecycle_state(SEC_TE_MAILBOX_BASE);
			if (lifecycle != ADI_LIFECYCLE_ADI_PROV_ENC && lifecycle != ADI_LIFECYCLE_CUST1_PROV_HOST)
				return TEE_ERROR_GENERIC;
		}
		return TEE_SUCCESS;
	default:
		/* For all other commands, the lifecycle state doesn't need to be verified */
		return TEE_SUCCESS;
	}

	return TEE_ERROR_GENERIC;
}

static TEE_Result te_mailbox(uint32_t cmd, TEE_Param params[4])
{
	int ret = -1;
	host_keys_t key_struct[1];
	uint8_t *key_buf = NULL;
	uint32_t key_len = params[0].memref.size;

	switch (cmd) {
	case PROV_HOST_KEY_CMD:
		/* Verify key id and key size */
		switch (params[1].value.a) {
		case HST_SEC_DEBUG:
		case HST_SEC_BOOT:
		case HST_PLLSA:
			if (key_len != 32) {
				plat_runtime_error_message("Invalid key size");
				return TEE_ERROR_BAD_PARAMETERS;
			}
			break;
		case HST_IPK:
			if (key_len != 16) {
				plat_runtime_error_message("Invalid key size");
				return TEE_ERROR_BAD_PARAMETERS;
			}
			break;
		default:
			plat_runtime_error_message("Invalid key id");
			return TEE_ERROR_BAD_PARAMETERS;
		}

		/* Copy key to buffer */
		key_buf = malloc(key_len);

		if (key_buf == NULL) {
			plat_runtime_error_message("Error creating buffer for key to provision");
			return TEE_ERROR_GENERIC;
		}

		memcpy(key_buf, params[0].memref.buffer, key_len);

		/* Setup structure for key */
		key_struct[0].hst_key_id = params[1].value.a;
		key_struct[0].key_len = key_len;
		key_struct[0].key = key_buf;

		/* Pass key structure to TE mailbox API call to provision host key */
		ret = adi_enclave_provision_host_keys(TE_MAILBOX_BASE, (uintptr_t)key_struct, (uint32_t)(sizeof(key_struct) / sizeof(*key_struct)), (uint32_t)sizeof(key_struct));

		/* Check status from TE mailbox API */
		if (ret != 0)
			break;

		/* Provision host keys on secondary, if dual-tile */
		if (plat_is_dual_tile())
			ret = adi_enclave_provision_host_keys(SEC_TE_MAILBOX_BASE, (uintptr_t)key_struct, (uint32_t)(sizeof(key_struct) / sizeof(*key_struct)), (uint32_t)sizeof(key_struct));

		break;
	case PROV_PREP_FINALIZE_CMD:
		/* Prepare finalize provisioning */
		ret = adi_enclave_provision_prepare_finalize(TE_MAILBOX_BASE);

		/* Check status from TE mailbox API */
		if (ret != 0)
			break;

		/* Prepare finalize provisioning on secondary, if dual-tile */
		if (plat_is_dual_tile())
			ret = adi_enclave_provision_prepare_finalize(SEC_TE_MAILBOX_BASE);

		break;
	case PROV_FINALIZE_CMD:
		ret = adi_enclave_provision_finalize(TE_MAILBOX_BASE);

		/* Check status from TE mailbox API */
		if (ret != 0)
			break;

		/* Finalize provisioning on secondary, if dual-tile */
		if (plat_is_dual_tile())
			ret = adi_enclave_provision_finalize(SEC_TE_MAILBOX_BASE);

		break;
	case BOOT_FLOW_REG_READ:
		params[OP_PARAM_BOOT_FLOW].value.a = adi_enclave_get_boot_flow0(TE_MAILBOX_BASE);;
		params[OP_PARAM_BOOT_FLOW].value.b = adi_enclave_get_boot_flow1(TE_MAILBOX_BASE);

		return TEE_SUCCESS;
		break;
	default:
		plat_runtime_error_message("Invalid TE Mailbox API");
		free(key_buf);
		return TEE_ERROR_BAD_PARAMETERS;
		break;
	}

	/* Check status from TE mailbox API */
	if (ret != 0) {
		IMSG("TE Mailbox API returned an error: %x\n", ret);
		free(key_buf);
		return TEE_ERROR_GENERIC;
	}

	free(key_buf);

	return TEE_SUCCESS;
}

/*
 * Trusted Application Entry Points
 */
static TEE_Result invoke_command(void *psess __unused,
				 uint32_t cmd, uint32_t ptypes,
				 TEE_Param params[TEE_NUM_PARAMS] __unused)
{
	/* Check parameters */
	if (adi_te_mailbox_check_params(ptypes, cmd) != TEE_SUCCESS)
		return TEE_ERROR_BAD_PARAMETERS;

	/* Check lifecycle state */
	if (adi_te_mailbox_check_lifecycle_state(cmd) != TEE_SUCCESS)
		return TEE_ERROR_BAD_STATE;

	return te_mailbox(cmd, params);
}

pseudo_ta_register(.uuid = TE_MAILBOX_PTA_UUID, .name = TA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command);
