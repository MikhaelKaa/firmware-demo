/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright 2026 Michael Kaa */

/**
 * @file ucmd_stub.c
 * @brief Минимальный stub ucmd_parse для хост-тестов.
 *
 * Полный ucmd.c зависит от CMSIS (stm32f407xx.h через NVIC_SystemReset),
 * поэтому для хост-компиляции предоставляем только ucmd_parse.
 */

#include "ucmd.h"
#include <string.h>

/**
 * @brief Parse and dispatch a command against a command table.
 *
 * Копия из firmware/cli/u_read_line/ucmd.c без CMSIS зависимостей.
 */
int ucmd_parse(command_t cmd_list[], int argc, const char **argv)
{
    if (!argv) return 0;
    if (!cmd_list) return UCMD_CMD_NOT_FOUND;

    int retval = 0;

    if (argc) {
        command_t *c = NULL;
        for (command_t *p = cmd_list; p->cmd; p++) {
            if (strcmp(p->cmd, argv[0]) == 0) {
                c = p;
            }
        }
        if (c) {
            retval = c->fn(argc, (char **)argv);
        } else {
            retval = UCMD_CMD_NOT_FOUND;
        }
    }

    return retval;
}