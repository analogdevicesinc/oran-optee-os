/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2023 Analog Devices Incorporated
 */

#ifndef ADRV906X_UTIL_H
#define ADRV906X_UTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <tee_internal_api.h>

bool plat_is_dual_tile(void);
bool plat_is_secondary_linux_enabled(void);

TEE_Result plat_set_enforcement_counter(void);
TEE_Result plat_set_te_enforcement_counter(void);
TEE_Result plat_get_enforcement_counter(uint32_t param_types, TEE_Param params[TEE_NUM_PARAMS]);
TEE_Result plat_get_te_enforcement_counter(uint32_t param_types, TEE_Param params[TEE_NUM_PARAMS]);

#endif /* ADRV906X_UTIL_H */
