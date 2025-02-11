/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2025 Analog Devices Incorporated
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>

void common_main_init_gic(void);

uint32_t plat_get_sysclk_freq(void);
uint32_t plat_get_anti_rollback_counter(void);
uint32_t plat_get_te_anti_rollback_counter(void);
void plat_error_message(const char *fmt, ...);
void plat_warn_message(const char *fmt, ...);
void plat_runtime_error_message(const char *fmt, ...);
void plat_runtime_warn_message(const char *fmt, ...);

#endif /* COMMON_H */
