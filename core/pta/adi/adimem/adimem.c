/*
 * Copyright (c) 2025, Analog Devices Inc.
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
#include <io.h>
#include <kernel/pseudo_ta.h>
#include <mm/core_memprot.h>
#include <tee_internal_api.h>

#include <drivers/adi/adi_te_interface.h>
#include <adrv906x_def.h>
#include <common.h>
#include "adimem.h"

#include <runtime_log.h>

#define TA_NAME "adimem.ta"

#define TA_ADIMEM_UUID \
	{ \
		0x23fd8eb3, \
		0xf9e6, 0x434c, \
		{ \
			0x94, 0xf2, \
			0xa9, 0x1a, \
			0x61, 0x38, \
			0xbf, 0x3d, \
		} \
	}

/* Op parameter offsets */
#define OP_PARAM_ADDR 0
#define OP_PARAM_SIZE 1
#define OP_PARAM_DATA 2
#define OP_PARAM_PRIV 3

/* The function IDs implemented in this TA */
enum ta_adimem_cmds {
	TA_ADIMEM_CMD_READ,
	TA_ADIMEM_CMD_WRITE,
	TA_ADIMEM_CMDS_COUNT
};

/*
 * adimem_check_params - verify the received parameters are of the expected types, and size parameter is valid
 */
static TEE_Result adimem_check_params(uint32_t param_types, TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_VALUE_INOUT, TEE_PARAM_TYPE_VALUE_INPUT);
	size_t size = params[OP_PARAM_SIZE].value.a;

	if (param_types != exp_param_types) {
		plat_runtime_error_message("Bad parameters");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	switch (size) {
	case 8:  break;
	case 16: break;
	case 32: break;
	default:
		plat_runtime_error_message("%s Invalid data size '%lu'", TA_NAME, size);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	return TEE_SUCCESS;
}

/*
 * adimem_read_handler - manages the read requests
 */
static TEE_Result adimem_read_handler(TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t address = params[OP_PARAM_ADDR].value.a;
	size_t size = params[OP_PARAM_SIZE].value.a;
	vaddr_t base;
	bool base_is_new_mmu_map = false;
	uint32_t value;
	bool ok;

	/* Remap */
	base = (vaddr_t)phys_to_virt_io(address, sizeof(uint32_t));
	if (!base) {
		/* MMU add mapping, to support addresses not registered with "register_phys_mem" */
		base = (vaddr_t)core_mmu_add_mapping(MEM_AREA_IO_SEC, address, size);
		if (!base) {
			plat_runtime_error_message("%s READ  MMU address mapping failure", TA_NAME);
			return TEE_ERROR_GENERIC;
		}
		base_is_new_mmu_map = true;
	}

	/* Read value */
	switch (size) {
	case 8:  ok = true; value = io_read8(base); break;
	case 16: ok = true; value = io_read16(base); break;
	case 32: ok = true; value = io_read32(base); break;
	default: ok = false; break;
	}

	if (base_is_new_mmu_map) {
		/* MMU remove mapping */
		if (core_mmu_remove_mapping(MEM_AREA_IO_SEC, (void *)base, size) != TEE_SUCCESS) {
			plat_runtime_error_message("%s READ MMU address unmapping failure", TA_NAME);
			return TEE_ERROR_GENERIC;
		}
	}

	if (!ok)
		return TEE_ERROR_GENERIC;

	/* Save value */
	params[OP_PARAM_DATA].value.a = value;

	IMSG("%s READ address 0x%08x value 0x%x", TA_NAME, address, value);

	return TEE_SUCCESS;
}

/*
 * adimem_write_handler - manages the write requests
 */
static TEE_Result adimem_write_handler(TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t address = params[OP_PARAM_ADDR].value.a;
	size_t size = params[OP_PARAM_SIZE].value.a;
	vaddr_t base;
	bool base_is_new_mmu_map = false;
	uint32_t value;
	bool ok;

	/* Remap */
	base = (vaddr_t)phys_to_virt_io(address, sizeof(uint32_t));
	if (!base) {
		/* MMU add mapping, to support addresses not registered with "register_phys_mem" */
		base = (vaddr_t)core_mmu_add_mapping(MEM_AREA_IO_SEC, address, size);
		if (!base) {
			plat_runtime_error_message("%s WRITE MMU address mapping failure", TA_NAME);
			return TEE_ERROR_GENERIC;
		}
		base_is_new_mmu_map = true;
	}

	/* Write value */
	value = params[OP_PARAM_DATA].value.a;
	switch (size) {
	case 8:  ok = true;  io_write8(base, value); break;
	case 16: ok = true; io_write16(base, value); break;
	case 32: ok = true; io_write32(base, value); break;
	default: ok = false; break;
	}

	if (base_is_new_mmu_map) {
		/* MMU remove mapping */
		if (core_mmu_remove_mapping(MEM_AREA_IO_SEC, (void *)base, size) != TEE_SUCCESS) {
			plat_runtime_error_message("%s WRITE MMU address unmapping failure", TA_NAME);
			return TEE_ERROR_GENERIC;
		}
	}

	if (!ok)
		return TEE_ERROR_GENERIC;

	IMSG("%s WRITE address 0x%08x value 0x%x", TA_NAME, address, value);

	return TEE_SUCCESS;
}

/*
 * adimem_verify_access - Checks a given request against the access control list.
 * Returns true if access is allowed, false otherwise.
 */
static bool adimem_verify_access(uint32_t cmd, TEE_Param params[TEE_NUM_PARAMS])
{
	const adimem_entry_t *access_table;
	uint32_t addr;
	const adimem_entry_t *cur_entry;
	size_t cur_entry_idx;
	const size_t num_entries = get_access_table_num_entries();
	size_t size;

	access_table = get_access_table();

	addr = params[OP_PARAM_ADDR].value.a;
	size = params[OP_PARAM_SIZE].value.a;

	/* Check the given request against each entry in the access table */
	for (cur_entry_idx = 0; cur_entry_idx < num_entries; cur_entry_idx++) {
		cur_entry = &(access_table[cur_entry_idx]);
		if (cur_entry->address == addr) {
			if (cur_entry->size == size) {
				if (cmd == TA_ADIMEM_CMD_READ) {
					if ((cur_entry->type & ADIMEM_ACCESS_TYPE_READ) == ADIMEM_ACCESS_TYPE_READ)
						return true;
				} else if (cmd == TA_ADIMEM_CMD_WRITE) {
					if ((cur_entry->type & ADIMEM_ACCESS_TYPE_WRITE) == ADIMEM_ACCESS_TYPE_WRITE)
						return true;
				}
			}
		}
	}

	return false;
}

/*
 * Trusted Application Entry Points
 */
static TEE_Result invoke_command(void *psess __unused,
				 uint32_t cmd, uint32_t ptypes,
				 TEE_Param params[TEE_NUM_PARAMS])
{
	bool is_privileged = false;
	bool access_allowed = false;

	/* Check command */
	if (cmd != TA_ADIMEM_CMD_READ && cmd != TA_ADIMEM_CMD_WRITE) {
		plat_runtime_error_message("Invalid command");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Check parameters */
	if (adimem_check_params(ptypes, params) != TEE_SUCCESS)
		return TEE_ERROR_BAD_PARAMETERS;

	/* Command is considered privileged if all of the following are true:
	 * 1) This is a debug build (DEBUG is set)
	 * 2) The device lifecycle state is pre-deployed
	 * 3) The "privileged" flag was set by the caller
	 */
	is_privileged = false;
#ifdef DEBUG
#if DEBUG
	if (adi_enclave_get_lifecycle_state(TE_MAILBOX_BASE) < ADI_LIFECYCLE_DEPLOYED)
		if (params[OP_PARAM_PRIV].value.a != 0)
			is_privileged = true;
#endif
#endif

	/* Trace command */
	IMSG("%s %s %s address 0x%08x size %u...",
	     TA_NAME,
	     is_privileged == true ? "PRIV" : "NON-PRIV",
	     cmd == TA_ADIMEM_CMD_READ ? "READ" : "WRITE",
	     params[OP_PARAM_ADDR].value.a,
	     params[OP_PARAM_SIZE].value.a);

	/* Check if access is allowed */
	access_allowed = (adimem_verify_access(cmd, params) == true);

	if (is_privileged) {
		/* If this is a privileged access, but privileged access is not required,
		 * deny the access. This is intended to catch ADI applications that need
		 * adimem for mission mode, but are running as root (and should instead
		 * be running as non-root).
		 */
		if (access_allowed) {
			plat_runtime_error_message("Access denied. Re-run as non-privileged.");
			return TEE_ERROR_BAD_PARAMETERS;
		}

		/* Allow all accesses for privileged mode */
		access_allowed = true;
	}

	if (!access_allowed) {
		plat_runtime_error_message("Access denied.");
		return TEE_ERROR_ACCESS_DENIED;
	}

	switch (cmd) {
	case TA_ADIMEM_CMD_READ:  return adimem_read_handler(params);
	case TA_ADIMEM_CMD_WRITE: return adimem_write_handler(params);
	default: break;
	}
	return TEE_ERROR_BAD_PARAMETERS;
}

pseudo_ta_register(.uuid = TA_ADIMEM_UUID, .name = TA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command);
