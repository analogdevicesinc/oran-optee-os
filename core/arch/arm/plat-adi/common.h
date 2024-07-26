/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2024 Analog Devices Incorporated
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>

void common_main_init_gic(void);

uint32_t plat_get_anti_rollback_counter(void);
uint32_t plat_get_te_anti_rollback_counter(void);
bool plat_get_bootrom_bypass_enabled(void);

#endif /* COMMON_H */
