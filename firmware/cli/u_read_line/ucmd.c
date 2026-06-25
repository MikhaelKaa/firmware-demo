/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright 2026 Michael Kaa */

#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "term_gxf.h"
#include "ucmd.h"
#include "microrl.h"
#include "drv_face.h"
#include "uart.h"

#include "memory_man.h"
#include "ucmd_time.h"
#include "ucmd_led.h"
#include "ucmd_w25q.h"
#include "ucmd_adc.h"

/* ------------------------------------------------------------------ */
/* External declarations                                              */
/* ------------------------------------------------------------------ */

extern int ucmd_usb(int argc, char *argv[]);

/* ------------------------------------------------------------------ */
/* MCU reset command (Controlled layer violation)                      */
/* ------------------------------------------------------------------ */
/*
 * NOTE: Controlled layer violation — this function directly uses CMSIS
 * (NVIC_SystemReset). Planned for migration to a dedicated module
 * in a future refactoring cycle.
 */
#include "stm32f407xx.h"

static int ucmd_mcu_reset(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    NVIC_SystemReset();
    return 0; /* never reached, silences compiler warning */
}

/* ------------------------------------------------------------------ */
/* Command table                                                      */
/* ------------------------------------------------------------------ */

static command_t cmd_list[] = {
    {
        .cmd  = "help",
        .help = "print available commands with their help text",
        .fn   = (command_cb)ucmd_help,
    },
    {
        .cmd  = "reset",
        .help = "reset mcu",
        .fn   = (command_cb)ucmd_mcu_reset,
    },
    {
        .cmd  = "mem",
        .help = "memory man, use mem help",
        .fn   = (command_cb)ucmd_mem,
    },
    {
        .cmd  = "time",
        .help = "rtc time. to set type time hh mm ss",
        .fn   = (command_cb)ucmd_time,
    },
    {
        .cmd  = "led",
        .help = "led PA1 ctrl",
        .fn   = (command_cb)ucmd_led,
    },
    {
        .cmd  = "w25q",
        .help = "w25q ctrl",
        .fn   = (command_cb)ucmd_w25q,
    },
    {
        .cmd  = "usb",
        .help = "usb commands, use 'usb help'",
        .fn   = (command_cb)ucmd_usb,
    },
    {
        .cmd  = "adc",
        .help = "adc commands, use 'adc help'",
        .fn   = (command_cb)ucmd_adc,
    },
    {0}, /* null list terminator — DO NOT REMOVE */
};

/* ------------------------------------------------------------------ */
/* Command parsing                                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief Parse and dispatch a command against a command table.
 *
 * @param cmd_list  Array of command_t entries (must be NULL-terminated).
 * @param argc      Argument count.
 * @param argv      Argument vector (const strings).
 * @return 0 on success, UCMD_CMD_NOT_FOUND if no match, or the callback's
 *         return value.
 */
int ucmd_parse(command_t cmd_list[], int argc, const char **argv)
{
    if (!argv) return 0;            /* return 0 for empty commands */
    if (!cmd_list) return UCMD_CMD_NOT_FOUND; /* obviously not found */

    int retval = 0;

    if (argc) {
        command_t *c = NULL;
        for (command_t *p = cmd_list; p->cmd; p++)
            if (strcmp(p->cmd, argv[0]) == 0) c = p;
        if (c) retval = c->fn(argc, (char **)argv);
        else retval = UCMD_CMD_NOT_FOUND;
    }

    return retval;
}

/* ------------------------------------------------------------------ */
/* Internal state                                                     */
/* ------------------------------------------------------------------ */

static microrl_t          default_rl;
static const drv_face_t  *ucmd_iface;

/* ------------------------------------------------------------------ */
/* Command execution callback                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief Execute a command by looking it up in the command table.
 *
 * If the command is not found, prints an error message. After six
 * consecutive unknown commands a humorous warning is displayed
 * ("GO SLEEP, STUPID USER!") — an easter egg inherited from the
 * ZX-Evolution Pentevo project (NedoPC).
 *
 * @param argc  Argument count.
 * @param argv  Argument vector.
 * @return 0 on success, UCMD_CMD_NOT_FOUND if command not found.
 */
static int ucmd_execute(int argc, char **argv)
{
    int ret = ucmd_parse(cmd_list, argc, (const char **)argv);

    if (ret == UCMD_CMD_NOT_FOUND) {
        static uint8_t gssu = 0;
        if (gssu++ < 6) {
            printf("unknown command");
            for (int i = 0; i < argc; i++) {
                printf(" %s", argv[i]);
            }
            printf(", try help\r\n");
        } else {
            gssu = 0;
            set_display_atrib(F_RED);
            printf("GO SLEEP, STUPID USER!\r\n");
            resetcolor();
        }
    }

    return ret;
}

/* ------------------------------------------------------------------ */
/* Help callback                                                      */
/* ------------------------------------------------------------------ */

/** @brief Print the list of available commands with help text. */
int ucmd_help(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    command_t *p = cmd_list;
    while (p->cmd) {
        printf("%s \t%s\r\n", p->cmd, p->help);
        p++;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Print callback                                                     */
/* ------------------------------------------------------------------ */

/** @brief Default microrl print callback (delegates to printf). */
void ucmd_default_print(const char *str)
{
    printf("%s", str);
}

/* ------------------------------------------------------------------ */
/* SIGINT handler                                                     */
/* ------------------------------------------------------------------ */

static void default_sigint(void)
{
    printf("default_sigint\r\n");
}

/* ------------------------------------------------------------------ */
/* Initialization                                                     */
/* ------------------------------------------------------------------ */

/** @brief Initialize the default CLI handler. */
void ucmd_default_init(void)
{
    /* Disable stdio buffering so echo and input work correctly */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    ucmd_iface = dev_uart1_get();
    microrl_init(&default_rl, ucmd_default_print);
    microrl_set_execute_callback(&default_rl,
                                 (int (*)(int, const char * const *))ucmd_execute);
    microrl_set_sigint_callback(&default_rl, default_sigint);
    microrl_insert_char(&default_rl, '\n');
    microrl_insert_char(&default_rl, '\n');
}

/* ------------------------------------------------------------------ */
/* Processing loop                                                    */
/* ------------------------------------------------------------------ */

/** @brief Process one character of input (call from main loop). */
void ucmd_default_proc(void)
{
    /* Read one byte from the configured interface. */
    uint8_t ch;
    int ret = ucmd_iface->read(&ch, 1);
    if (ret <= 0) return;
    microrl_insert_char(&default_rl, ch);
}

/* ------------------------------------------------------------------ */
/* SIGINT override                                                    */
/* ------------------------------------------------------------------ */

/** @brief Replace the default SIGINT handler at runtime. */
void ucmd_set_sigint(void (*sigintf)(void))
{
    microrl_set_sigint_callback(&default_rl, sigintf);
}