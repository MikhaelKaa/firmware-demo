/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright 2026 Michael Kaa */

#include "reset.h"
#include "ucmd_reset.h"

/**
 * @brief CLI-команда reset — обёртка над драйвером reset_perform().
 *
 * Вызывает аппаратный сброс MCU через драйвер. Функция не возвращается.
 *
 * @param argc  Argument count (unused).
 * @param argv  Argument vector (unused).
 * @return 0 on success (never reached, reset does not return).
 */
int ucmd_reset(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    reset_perform();
    return 0; /* never reached, silences compiler warning */
}