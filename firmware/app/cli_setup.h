/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright 2026 Michael Kaa */

#ifndef _CLI_SETUP_H_
#define _CLI_SETUP_H_

#include "cli.h"

/** Recommended memory size for a CLI instance (matches cli.c definition). */
#define CLI_INSTANCE_SIZE 768

/**
 * @brief Get the CLI configuration for the default UART1 console.
 *
 * Returns a pre-built cli_config_t with the command table assembled
 * from all registered CLI modules.
 *
 * @return Pointer to cli_config_t (static, lives for the lifetime of the app).
 */
const cli_config_t *cli_setup_get_config(void);

/**
 * @brief Initialize CLI using the old single-instance API (Round 0/1 compat).
 *
 * This function is kept for backward compatibility. It internally calls
 * ucmd_default_init_with_commands() with the assembled command table.
 */
void cli_setup_init(void);

#endif /* _CLI_SETUP_H_ */