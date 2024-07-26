/*
 * Copyright (c) 2023, Analog Devices Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>

#include <io.h>
#include <kernel/pseudo_ta.h>
#include <mm/core_memprot.h>
#include <tee_internal_api.h>

#include <adrv906x_def.h>

#include <drivers/adi/adi_otp.h>
#include <drivers/adi/adrv906x/adi_adrv906x_otp.h>

#define TA_NAME "otp_macs.ta"

/*
 * This UUID is generated with uuidgen
 * the ITU-T UUID generator at http://www.itu.int/ITU-T/asn1/uuid.html
 */
#define TA_OTP_MACS_UUID \
	{ \
		0x61e8b041, \
		0xc3bc, 0x4b70, \
		{ \
			0xa9, 0x9e, \
			0xd2, 0xe5, \
			0xba, 0x2c, \
			0x4e, 0xbf, \
		} \
	}

/* Op parameter offsets */
#define OP_PARAM_INTERFACE      0
#define OP_PARAM_MAC_VALUE      1

/* The function IDs implemented in this TA */
enum ta_otp_macs_cmds {
	TA_OTP_MACS_CMD_READ,
	TA_OTP_MACS_CMD_WRITE,
	TA_OTP_MACS_CMDS_COUNT
};

/*
 * Check if a mac is all-0 bytes
 */
static bool mac_is_all_zeros(const uint8_t *mac)
{
	for (size_t i = 0; i < MAC_ADDRESS_NUM_BYTES; i++)
		if (mac[i] != 0x00) return false;
	return true;
}

/*
 * otp_macs_check_params - verify the received parameters are of the expected types, and MAC id parameter is valid
 */
static TEE_Result otp_macs_check_params(uint32_t param_types, TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_VALUE_INOUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);
	uint8_t interface;

	if (param_types != exp_param_types) {
		EMSG("%s Bad parameters", TA_NAME);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	interface = params[OP_PARAM_INTERFACE].value.a;
	if (interface == 0 || interface > NUM_MAC_ADDRESSES) {
		EMSG("%s Invalid MAC id '%u' (expected 1-%u)", TA_NAME, interface, NUM_MAC_ADDRESSES);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	return TEE_SUCCESS;
}

/*
 * otp_macs_read_handler - manages the read requests
 */
static TEE_Result otp_macs_read_handler(TEE_Param params[TEE_NUM_PARAMS])
{
	vaddr_t base;
	uint8_t interface = params[OP_PARAM_INTERFACE].value.a;
	uint8_t mac[MAC_ADDRESS_NUM_BYTES];
	bool base_is_new_mmu_map = false;
	int ret;

	base = (vaddr_t)phys_to_virt_io(OTP_BASE, SMALL_PAGE_SIZE);
	if (!base) {
		/* MMU add mapping, to support addresses not registered with "register_phys_mem" */
		base = (vaddr_t)core_mmu_add_mapping(MEM_AREA_IO_SEC, OTP_BASE, SMALL_PAGE_SIZE);
		if (!base) {
			EMSG("%s READ MMU address mapping failure", TA_NAME);
			return TEE_ERROR_GENERIC;
		}
		base_is_new_mmu_map = true;
	}

	ret = adrv906x_otp_get_mac_addr(base, interface, mac);

	if (base_is_new_mmu_map) {
		/* MMU remove mapping */
		if (core_mmu_remove_mapping(MEM_AREA_IO_SEC, (void *)base, SMALL_PAGE_SIZE) != TEE_SUCCESS) {
			EMSG("%s READ MMU address unmapping failure", TA_NAME);
			return TEE_ERROR_GENERIC;
		}
	}

	if (ret != ADI_OTP_SUCCESS)
		return TEE_ERROR_GENERIC;

	params[OP_PARAM_MAC_VALUE].value.a = mac[0] << 8 | mac[1];
	params[OP_PARAM_MAC_VALUE].value.b = mac[2] << 24 | mac[3] << 16 | mac[4] << 8 | mac[5];

	IMSG("%s READ MAC address %u", TA_NAME, interface);

	return TEE_SUCCESS;
}

/*
 * otp_macs_write_handler - manages the write requests
 */
static TEE_Result otp_macs_write_handler(TEE_Param params[TEE_NUM_PARAMS])
{
	vaddr_t base;
	uint8_t interface = params[OP_PARAM_INTERFACE].value.a;
	uint8_t mac[MAC_ADDRESS_NUM_BYTES];
	uint8_t otp_mac[MAC_ADDRESS_NUM_BYTES];
	bool base_is_new_mmu_map = false;
	int ret;

	mac[0] = params[OP_PARAM_MAC_VALUE].value.a >> 8 & 0xFF;
	mac[1] = params[OP_PARAM_MAC_VALUE].value.a >> 0 & 0xFF;
	mac[2] = params[OP_PARAM_MAC_VALUE].value.b >> 24 & 0xFF;
	mac[3] = params[OP_PARAM_MAC_VALUE].value.b >> 16 & 0xFF;
	mac[4] = params[OP_PARAM_MAC_VALUE].value.b >> 8 & 0xFF;
	mac[5] = params[OP_PARAM_MAC_VALUE].value.b >> 0 & 0xFF;

	base = (vaddr_t)phys_to_virt_io(OTP_BASE, SMALL_PAGE_SIZE);
	if (!base) {
		/* MMU add mapping, to support addresses not registered with "register_phys_mem" */
		base = (vaddr_t)core_mmu_add_mapping(MEM_AREA_IO_SEC, OTP_BASE, SMALL_PAGE_SIZE);
		if (!base) {
			EMSG("%s WRITE MMU address mapping failure", TA_NAME);
			return TEE_ERROR_GENERIC;
		}
		base_is_new_mmu_map = true;
	}

	/* Check no MAC is already stored in OTP */
	ret = adrv906x_otp_get_mac_addr(base, interface, otp_mac);
	if (ret == ADI_OTP_SUCCESS) {
		if (mac_is_all_zeros(otp_mac)) {
			/* No MAC in OTP, we can store the new MAC */
			ret = adrv906x_otp_set_mac_addr(base, interface, mac);
		} else {
			EMSG("%s: OTP already contains a MAC for interface %u. MAC write aborted\n", TA_NAME, interface);
			ret = ADI_OTP_FAILURE;
		}
	}

	if (base_is_new_mmu_map) {
		/* MMU remove mapping */
		if (core_mmu_remove_mapping(MEM_AREA_IO_SEC, (void *)base, SMALL_PAGE_SIZE) != TEE_SUCCESS) {
			EMSG("%s WRITE MMU address unmapping failure", TA_NAME);
			return TEE_ERROR_GENERIC;
		}
	}

	if (ret != ADI_OTP_SUCCESS)
		return TEE_ERROR_GENERIC;

	IMSG("%s WRITE MAC address %u", TA_NAME, interface);

	return TEE_SUCCESS;
}

/*
 * Trusted Application Entry Points
 */
static TEE_Result invoke_command(void *psess __unused,
				 uint32_t cmd, uint32_t ptypes,
				 TEE_Param params[TEE_NUM_PARAMS])
{
	/* Check command */
	if (cmd != TA_OTP_MACS_CMD_READ && cmd != TA_OTP_MACS_CMD_WRITE) {
		EMSG("Invalid command");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Check parameters */
	if (otp_macs_check_params(ptypes, params) != TEE_SUCCESS)
		return TEE_ERROR_BAD_PARAMETERS;

	switch (cmd) {
	case TA_OTP_MACS_CMD_READ:  return otp_macs_read_handler(params);
	case TA_OTP_MACS_CMD_WRITE: return otp_macs_write_handler(params);
	default: break;
	}
	return TEE_ERROR_BAD_PARAMETERS;
}

pseudo_ta_register(.uuid = TA_OTP_MACS_UUID, .name = TA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command);
