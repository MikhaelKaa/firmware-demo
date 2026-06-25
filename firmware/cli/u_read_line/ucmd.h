/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright 2026 Michael Kaa */

#ifndef _UCMD_H_
#define _UCMD_H_

#include <limits.h>
#include <stddef.h>

/** Returned by ucmd_parse() when the command is not found in the table. */
#define UCMD_CMD_NOT_FOUND INT_MIN

/**
 * @brief Command callback function signature.
 *
 * @param argc  Number of arguments (including command name).
 * @param argv  Array of argument strings (NULL-terminated by convention,
 *              but only the first argc elements are valid).
 * @return 0 on success, or a negative error code.
 */
typedef int (*command_cb)(int argc, char *argv[]);

/**
 * @brief Description of a single CLI command.
 *
 * Each entry in the command table associates a text keyword with help
 * information and a callback function that handles the command.
 */
typedef struct command {
    const char *cmd;    /**< the command string to match against        */
    const char *help;   /**< the help text associated with cmd          */
    command_cb fn;      /**< the function to call when cmd is matched   */
} command_t;

/**
 * @brief Initialize the default CLI handler.
 *
 * Sets up the internal microrl instance, registers execute and SIGINT
 * callbacks, and prints an initial prompt. Call once during startup.
 */
void ucmd_default_init(void);

/**
 * @brief Process pending input in the main loop.
 *
 * Reads one character from the configured input interface (if available
 * and rate-limit allows) and passes it to the microrl parser.
 */
void ucmd_default_proc(void);

/**
 * @brief Parse and dispatch a command against a command table.
 *
 * @param cmd_list  Array of command_t entries (must be NULL-terminated).
 * @param argc      Argument count.
 * @param argv      Argument vector (const strings).
 * @return 0 on success, UCMD_CMD_NOT_FOUND if no match, or the callback's
 *         return value.
 */
int ucmd_parse(command_t cmd_list[], int argc, const char **argv);

/**
 * @brief Print the list of available commands with help text.
 *
 * This is the default implementation of the "help" command callback.
 *
 * @param argc  Argument count (unused).
 * @param argv  Argument vector (unused).
 * @return 0 (always succeeds).
 */
int ucmd_help(int argc, char *argv[]);

/**
 * @brief Default print callback for microrl.
 *
 * Outputs the given string via printf.
 *
 * @param str  Null-terminated string to output.
 */
void ucmd_default_print(const char *str);

/**
 * @brief Override the SIGINT (Ctrl+C) handler.
 *
 * @param sigintf  New handler function (called with no arguments).
 */
void ucmd_set_sigint(void (*sigintf)(void));

#endif /* _UCMD_H_ */