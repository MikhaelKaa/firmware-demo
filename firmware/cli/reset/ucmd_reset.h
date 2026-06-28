/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright 2026 Michael Kaa */

#ifndef _UCMD_RESET_H_
#define _UCMD_RESET_H_

/**
 * @brief CLI-команда reset (обёртка над драйвером reset_perform).
 *
 * @param argc  Argument count (unused).
 * @param argv  Argument vector (unused).
 * @return 0 on success (never reached, reset does not return).
 */
int ucmd_reset(int argc, char *argv[]);

#endif /* _UCMD_RESET_H_ */