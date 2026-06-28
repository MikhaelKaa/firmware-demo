/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright 2026 Michael Kaa */

#include "cli_setup.h"
#include "ucmd.h"

#include "memory_man.h"
#include "ucmd_time.h"
#include "ucmd_led.h"
#include "ucmd_w25q.h"
#include "ucmd_adc.h"
#include "ucmd_reset.h"

/* ------------------------------------------------------------------ */
/* External declarations                                              */
/* ------------------------------------------------------------------ */

extern int ucmd_usb(int argc, char *argv[]);

/* ------------------------------------------------------------------ */
/* Command table (cli_command_t with privilege levels)                */
/* ------------------------------------------------------------------ */

static const cli_command_t g_cli_commands[] = {
    { .cmd = "help",  .help = "print available commands with their help text", .fn = (command_cb)ucmd_help,  .privilege_level = CLI_PRIV_PUBLIC },
    { .cmd = "reset", .help = "reset MCU",                                    .fn = (command_cb)ucmd_reset, .privilege_level = CLI_PRIV_PUBLIC },
    { .cmd = "mem",   .help = "memory man, use mem help",                     .fn = (command_cb)ucmd_mem,   .privilege_level = CLI_PRIV_PUBLIC },
    { .cmd = "time",  .help = "RTC time. to set type time hh mm ss",          .fn = (command_cb)ucmd_time,  .privilege_level = CLI_PRIV_PUBLIC },
    { .cmd = "led",   .help = "LED PA1 ctrl",                                 .fn = (command_cb)ucmd_led,   .privilege_level = CLI_PRIV_PUBLIC },
    { .cmd = "w25q",  .help = "W25Q flash control",                           .fn = (command_cb)ucmd_w25q,  .privilege_level = CLI_PRIV_PUBLIC },
    { .cmd = "usb",   .help = "USB commands, use 'usb help'",                 .fn = (command_cb)ucmd_usb,   .privilege_level = CLI_PRIV_PUBLIC },
    { .cmd = "adc",   .help = "ADC commands, use 'adc help'",                 .fn = (command_cb)ucmd_adc,   .privilege_level = CLI_PRIV_PUBLIC },
    {0}, /* null terminator */
};

/* ------------------------------------------------------------------ */
/* CLI configuration (for cli_create API)                             */
/* ------------------------------------------------------------------ */

/**
 * @brief Get the CLI configuration for the default UART1 console.
 *
 * Returns a pre-built cli_config_t with the command table assembled
 * from all registered CLI modules.
 */
const cli_config_t *cli_setup_get_config(void)
{
    static const cli_config_t g_config = {
        .iface     = NULL, /* Set by caller (dev_uart1_get())           */
        .commands  = g_cli_commands,
        .prompt    = NULL, /* Use default prompt                        */
        .auth_cb   = NULL, /* No authentication (open access)           */
        .auth_user_ctx = NULL,
        .sigint_cb = NULL, /* Default Ctrl+C behavior                   */
    };

    return &g_config;
}

/* ------------------------------------------------------------------ */
/* Legacy API (Round 0/1 backward compatibility)                      */
/* ------------------------------------------------------------------ */

/**
 * @brief Инициализация CLI (Раунд 1 — реальная таблица команд).
 *
 * Собирает таблицу команд из модулей app и передаёт её в CLI-ядро.
 * This wrapper is kept for backward compatibility with the old API.
 */
void cli_setup_init(void)
{
    /* Build a legacy command_t table from cli_command_t for ucmd compat */
    static const command_t g_legacy_commands[] = {
        { .cmd = "help",  .help = "print available commands with their help text", .fn = (command_cb)ucmd_help },
        { .cmd = "reset", .help = "reset MCU",                                    .fn = (command_cb)ucmd_reset },
        { .cmd = "mem",   .help = "memory man, use mem help",                     .fn = (command_cb)ucmd_mem },
        { .cmd = "time",  .help = "RTC time. to set type time hh mm ss",          .fn = (command_cb)ucmd_time },
        { .cmd = "led",   .help = "LED PA1 ctrl",                                 .fn = (command_cb)ucmd_led },
        { .cmd = "w25q",  .help = "W25Q flash control",                           .fn = (command_cb)ucmd_w25q },
        { .cmd = "usb",   .help = "USB commands, use 'usb help'",                 .fn = (command_cb)ucmd_usb },
        { .cmd = "adc",   .help = "ADC commands, use 'adc help'",                 .fn = (command_cb)ucmd_adc },
        {0}, /* null terminator */
    };

    ucmd_default_init_with_commands(g_legacy_commands);
}