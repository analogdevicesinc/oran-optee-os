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
#include <string.h>
#include <tee_internal_api.h>

#include <common.h>

#include "adi_memdump.h"

#define TA_NAME "adi_memdump.ta"

#define TA_ADI_MEMDUMP_UUID \
	{ \
		0x39f74b29, \
		0x8507, 0x4142, \
		{ \
			0x8b, 0x8e, \
			0x3d, 0x12, \
			0xeb, 0x9d, \
			0x49, 0x7b, \
		} \
	}

/* Op parameter offsets */
/* adi_memdump_get_num_records */
#define OP_PARAM_RECORDS 0

/* adi_memdump - size command */
#define OP_PARAM_RECORD_NUM 0
#define OP_PARAM_RECORD_SIZE 1

/* adi_memdump - size command */
#define OP_PARAM_BUFFER 0
#define OP_PARAM_RECORD_AND_ADDRESS 1
#define OP_PARAM_WIDTH 2
#define OP_PARAM_ENDIANNESS 3

/* The function IDs implemented in this TA */
enum ta_adi_memdump_cmds {
	TA_ADI_MEMDUMP_RECORDS_CMD,
	TA_ADI_MEMDUMP_SIZE_CMD,
	TA_ADI_MEMDUMP_CMD
};

/*
 * adi_memdump_check_params - verify the received parameters are of the expected types
 */
static TEE_Result adi_memdump_check_params(uint32_t param_types, uint32_t cmd)
{
	switch (cmd) {
	case TA_ADI_MEMDUMP_RECORDS_CMD:
		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE)) {
			plat_runtime_error_message("Bad parameters to memdump records command");
			return TEE_ERROR_BAD_PARAMETERS;
		} else {
			return TEE_SUCCESS;
		}
	case TA_ADI_MEMDUMP_SIZE_CMD:
		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_VALUE_OUTPUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE)) {
			plat_runtime_error_message("Bad parameters to memdump size command");
			return TEE_ERROR_BAD_PARAMETERS;
		} else {
			return TEE_SUCCESS;
		}
	case TA_ADI_MEMDUMP_CMD:
		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT, TEE_PARAM_TYPE_VALUE_INOUT, TEE_PARAM_TYPE_VALUE_OUTPUT, TEE_PARAM_TYPE_VALUE_OUTPUT)) {
			plat_runtime_error_message("Bad parameters to memdump command");
			return TEE_ERROR_BAD_PARAMETERS;
		} else {
			return TEE_SUCCESS;
		}
	default: break;
	}

	return TEE_ERROR_BAD_PARAMETERS;
}

/*
 * adi_memdump_get_num_records_handler - gets total number of records to memdump
 */
static TEE_Result adi_memdump_get_num_records_handler(TEE_Param params[TEE_NUM_PARAMS])
{
	params[OP_PARAM_RECORDS].value.a = get_num_records();

	return TEE_SUCCESS;
}

/*
 * valid_record_num - checks validity of record number
 */
static bool valid_record_num(uint32_t record_num)
{
	if (record_num < get_num_records())
		return true;

	return false;
}

/*
 * get_record_size - gets size in bytes of current record, accounts for dual-tile scenario
 */
static uint32_t get_record_size(uint32_t record_num)
{
	memdump_registers_t record;

	/* Check validity of record number */
	if (!valid_record_num(record_num))
		return 0;

	record = get_record(record_num);

	return record.cpu_mem_size;
}

/*
 * adi_memdump_get_record_size_handler - gets size in bytes of current record
 */
static TEE_Result adi_memdump_get_record_size_handler(TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t record_num = params[OP_PARAM_RECORD_NUM].value.a;

	/* Check validity of record number */
	if (!valid_record_num(record_num))
		return TEE_ERROR_BAD_PARAMETERS;

	/* Get record size */
	params[OP_PARAM_RECORD_SIZE].value.a = get_record_size(record_num);

	return TEE_SUCCESS;
}

/*
 * adi_memdump_handler - manages the memdump requests and dumps memory contents to shared buffer
 */
static TEE_Result adi_memdump_handler(TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t record_num = params[OP_PARAM_RECORD_AND_ADDRESS].value.a;
	uint32_t tmp_address = 0;
	memdump_registers_t record;
	void *base;
	bool base_is_new_mmu_map = false;

	/* Check validity of record number */
	if (!valid_record_num(record_num)) {
		plat_runtime_error_message("Invalid record number %d", record_num);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Get record */
	record = get_record(record_num);

	/* Verify record size is a multiple of width */
	if ((record.cpu_mem_size * 8 % record.cpu_mem_width) != 0) {
		plat_runtime_error_message("Size of record is not a multiple of width");
		return TEE_ERROR_GENERIC;
	}

	tmp_address = record.cpu_mem_addr;

	/* Iterate through address range by register width */
	for (unsigned int i = 0; i < record.cpu_mem_size * 8 / record.cpu_mem_width; i++) {
		/* Check if register is in exclusion list */
		if (is_address_excluded(tmp_address)) {
			/* Add 0s to buffer */
			if (record.cpu_mem_width == 64) {
				uint64_t value = 0;
				*((uint64_t *)params[OP_PARAM_BUFFER].memref.buffer + i) = value;
			} else if (record.cpu_mem_width == 32) {
				uint32_t value = 0;
				*((uint32_t *)params[OP_PARAM_BUFFER].memref.buffer + i) = value;
			} else if (record.cpu_mem_width == 16) {
				uint16_t value = 0;
				*((uint16_t *)params[OP_PARAM_BUFFER].memref.buffer + i) = value;
			} else if (record.cpu_mem_width == 8) {
				uint8_t value = 0;
				value = value & ~(get_bit_field_exclusion(tmp_address));
				*((uint8_t *)params[OP_PARAM_BUFFER].memref.buffer + i) = value;
			} else {
				/* Only supports 8, 16, and 32 bit registers */
				plat_runtime_error_message("Not a valid register width %d", record.cpu_mem_width);
				return TEE_ERROR_GENERIC;
			}

			/* Increment address pointer */
			tmp_address = tmp_address + record.cpu_mem_width / 8;
			continue;
		}

		/* Remap */
		base = phys_to_virt_io(tmp_address, record.cpu_mem_width);
		if (!base) {
			/* MMU add mapping, to support addresses not registered with "register_phys_mem" */
			base = core_mmu_add_mapping(MEM_AREA_IO_SEC, tmp_address, record.cpu_mem_width);
			if (!base) {
				plat_runtime_error_message("%s READ MMU address mapping failure", TA_NAME);
				return TEE_ERROR_GENERIC;
			}
			base_is_new_mmu_map = true;
		}

		/* Copy register contents to temp buffer */
		if (record.cpu_mem_width == 64) {
			uint64_t value;
			memcpy(&value, base, sizeof(uint64_t));
			/* If register is in the exclusion list, clear excluded bits */
			value = value & ~(get_bit_field_exclusion(tmp_address));
			*((uint64_t *)params[OP_PARAM_BUFFER].memref.buffer + i) = value;
		} else if (record.cpu_mem_width == 32) {
			uint32_t value;
			value = io_read32((vaddr_t)base);
			/* If register is in the exclusion list, clear excluded bits */
			value = value & ~(get_bit_field_exclusion(tmp_address));
			*((uint32_t *)params[OP_PARAM_BUFFER].memref.buffer + i) = value;
		} else if (record.cpu_mem_width == 16) {
			uint16_t value;
			value = io_read16((vaddr_t)base);
			/* If register is in the exclusion list, clear excluded bits */
			value = value & ~(get_bit_field_exclusion(tmp_address));
			*((uint16_t *)params[OP_PARAM_BUFFER].memref.buffer + i) = value;
		} else if (record.cpu_mem_width == 8) {
			uint8_t value;
			value = io_read8((vaddr_t)base);
			/* If register is in the exclusion list, clear excluded bits */
			value = value & ~(get_bit_field_exclusion(tmp_address));
			*((uint8_t *)params[OP_PARAM_BUFFER].memref.buffer + i) = value;
		} else {
			/* Only supports 8, 16, and 32 bit registers */
			plat_runtime_error_message("Not a valid register width %d", record.cpu_mem_width);

			/* Remove mmu mapping and return error */
			if (base_is_new_mmu_map)
				core_mmu_remove_mapping(MEM_AREA_IO_SEC, (void *)base, record.cpu_mem_width);
			return TEE_ERROR_GENERIC;
		}

		/* Increment address pointer */
		tmp_address = tmp_address + record.cpu_mem_width / 8;

		if (base_is_new_mmu_map) {
			/* MMU remove mapping */
			if (core_mmu_remove_mapping(MEM_AREA_IO_SEC, (void *)base, record.cpu_mem_width) != TEE_SUCCESS) {
				plat_runtime_error_message("%s READ  MMU address unmapping failure", TA_NAME);
				return TEE_ERROR_GENERIC;
			}
		}
	}

	/* Copy memory contents to output buffer and save output values */
	params[OP_PARAM_BUFFER].memref.size = record.cpu_mem_size;
	params[OP_PARAM_RECORD_AND_ADDRESS].value.a = record.cpu_mem_addr;
	params[OP_PARAM_WIDTH].value.a = record.cpu_mem_width;
	params[OP_PARAM_ENDIANNESS].value.a = record.cpu_mem_endianness;

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
	if (cmd != TA_ADI_MEMDUMP_RECORDS_CMD && cmd != TA_ADI_MEMDUMP_CMD && cmd != TA_ADI_MEMDUMP_SIZE_CMD) {
		plat_runtime_error_message("Invalid command");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Check parameters */
	if (adi_memdump_check_params(ptypes, cmd) != TEE_SUCCESS)
		return TEE_ERROR_BAD_PARAMETERS;

	switch (cmd) {
	case TA_ADI_MEMDUMP_RECORDS_CMD:
		IMSG("%s memdump get number of records command...", TA_NAME);
		return adi_memdump_get_num_records_handler(params);
	case TA_ADI_MEMDUMP_CMD:
		IMSG("%s memdump command, record number %d...", TA_NAME, params[OP_PARAM_RECORD_AND_ADDRESS].value.a);
		return adi_memdump_handler(params);
	case TA_ADI_MEMDUMP_SIZE_CMD:
		IMSG("%s memdump get size of record %d...", TA_NAME, params[OP_PARAM_RECORD_NUM].value.a);
		return adi_memdump_get_record_size_handler(params);
	default: break;
	}
	return TEE_ERROR_BAD_PARAMETERS;
}

pseudo_ta_register(.uuid = TA_ADI_MEMDUMP_UUID, .name = TA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command);
