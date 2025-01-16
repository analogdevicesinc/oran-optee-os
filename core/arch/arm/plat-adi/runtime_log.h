/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2025 Analog Devices Incorporated
 */

#ifndef RUNTIME_LOG_H
#define RUNTIME_LOG_H

#define SIZE_OF_OPTEE_RUNTIME_BUFFER 500

void write_to_runtime_buffer(const char *message);
void read_from_runtime_buffer(char *message, int size);
bool adi_runtime_log_smc(char *buffer, int size);

#endif /* RUNTIME_LOG_H */
