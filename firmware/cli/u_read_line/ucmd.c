/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright 2026 Michael Kaa */

#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "ucmd.h"
#include "microrl.h"
#include "drv_face.h"
#include "uart.h"

/* ------------------------------------------------------------------ */
/* Command table (external, set via ucmd_default_init_with_commands)   */
/* ------------------------------------------------------------------ */

static const command_t *g_cmd_table = NULL;

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
 /* Print callback adapter (printf-based, for backward compatibility)  */
/* ------------------------------------------------------------------ */

/** @brief Default microrl print callback (delegates to printf). */
void ucmd_default_print(const char *str)
{
    printf("%s", str);
}

/** @brief Microrl print callback with void* ctx (ignores ctx, uses printf). */
static void ucmd_default_print_ctx(const char *str, void *ctx)
{
    (void)ctx;
    printf("%s", str);
}

/* ------------------------------------------------------------------ */
/* Init with external command table                                   */
/* ------------------------------------------------------------------ */

void ucmd_default_init_with_commands(const command_t *commands)
{
    g_cmd_table = commands;
    ucmd_default_init();
}

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
    if (g_cmd_table == NULL) return -1;
    int ret = ucmd_parse((command_t *)g_cmd_table, argc, (const char **)argv);

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
            printf("\033[31mGO SLEEP, STUPID USER!\033[0m\r\n");
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

    /* Round 2: delegate to instance-based help via g_current_cli */
    extern int ucmd_help_compat(int argc, char *argv[]);
    return ucmd_help_compat(argc, argv);
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
    /* Require external command table to be set */
    if (g_cmd_table == NULL) return;

    /* Disable stdio buffering so echo and input work correctly */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    ucmd_iface = dev_uart1_get();
    microrl_init(&default_rl, ucmd_default_print_ctx, NULL);
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