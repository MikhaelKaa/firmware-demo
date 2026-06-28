/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright 2026 Michael Kaa */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <errno.h>

#include "cli.h"
#include "microrl.h"
#include "config.h"

/* ---------------------------------------------------------------
 * TX buffer size for formatted output
 * ---------------------------------------------------------------*/
#define CLI_TX_BUF_SIZE 256

/* Recommended memory size for a CLI instance */
#define CLI_INSTANCE_SIZE 768

/* ---------------------------------------------------------------
 * CLI instance (full definition)
 * ---------------------------------------------------------------*/
struct cli {
    microrl_t           rl;             /**< readline context            */
    const drv_face_t   *iface;          /**< I/O interface               */
    const cli_command_t *commands;      /**< command table               */
    char                tx_buf[CLI_TX_BUF_SIZE]; /**< output buffer     */
    cli_privilege_t     session_priv;   /**< current session privilege   */
    cli_auth_callback_t auth_cb;        /**< authentication hook         */
    void               *auth_user_ctx;  /**< context for auth_cb         */
    cli_proc_handler_t  proc_handler;   /**< hijack handler (NULL)       */
    cli_sigint_callback_t sigint_cb;    /**< Ctrl+C handler              */
    const char         *last_command;   /**< last executed command       */
};

/* ---------------------------------------------------------------
 * Print callback adapter (microrl -> drv_face_t->write)
 * ---------------------------------------------------------------*/

/**
 * @brief Microrl print callback that writes via cli->iface->write().
 * @param str  String to output.
 * @param ctx  Pointer to cli_t instance.
 */
static void cli_print_callback(const char *str, void *ctx)
{
    cli_t *cli = (cli_t *)ctx;
    if (cli && cli->iface && str) {
        size_t len = 0;
        /* Safe strlen: count until NUL or TX_BUF_SIZE */
        const char *p = str;
        while (*p && len < CLI_TX_BUF_SIZE) {
            p++;
            len++;
        }
        cli->iface->write(str, len);
    }
}

/* ---------------------------------------------------------------
 * Command lookup
 * ---------------------------------------------------------------*/

/**
 * @brief Find a command by name in the command table.
 * @return Pointer to the command entry or NULL if not found.
 */
static const cli_command_t *cli_find_command(const cli_t *cli, const char *name)
{
    if (!cli || !cli->commands || !name) return NULL;

    const cli_command_t *cmd = cli->commands;
    while (cmd->cmd) {
        if (strcmp(cmd->cmd, name) == 0) {
            return cmd;
        }
        cmd++;
    }
    return NULL;
}

/* ---------------------------------------------------------------
 * Command execution with privilege check
 * ---------------------------------------------------------------*/

/**
 * @brief Execute a command by looking it up in the command table.
 *
 * Checks privilege levels and calls auth_cb if needed. If the command
 * is not found, prints an error message. After six consecutive unknown
 * commands a humorous warning is displayed ("GO SLEEP, STUPID USER!")
 * — an easter egg inherited from the ZX-Evolution Pentevo project.
 */
static int cli_execute_command(cli_t *cli, int argc, const char *const *argv)
{
    if (!cli || !argv || !argc) return 0;

    /* Look up command in table */
    const cli_command_t *cmd = cli_find_command(cli, argv[0]);
    if (!cmd) {
        /* Command not found — print error */
        static uint8_t gssu_count = 0;
        if (gssu_count++ < 6) {
            cli_outputf(cli, "unknown command");
            for (int i = 0; i < argc; i++) {
                cli_outputf(cli, " %s", argv[i]);
            }
            cli_outputf(cli, ", try help\r\n");
        } else {
            gssu_count = 0;
            cli_outputf(cli, "\033[31mGO SLEEP, STUPID USER!\033[0m\r\n");
        }
        return UCMD_CMD_NOT_FOUND;
    }

    /* Save last command name for sigint context */
    cli->last_command = cmd->cmd;

    /* Check: is command disabled? */
    if (cmd->privilege_level == CLI_PRIV_DISABLED) {
        cli_outputf(cli, "command '%s' is disabled\r\n", cmd->cmd);
        return -EPERM;
    }

    /* Check privilege level */
    if (cmd->privilege_level > cli->session_priv) {
        if (cli->auth_cb != NULL) {
            cli_auth_result_t result = cli->auth_cb(
                cmd->cmd, cmd->privilege_level, cli->auth_user_ctx);

            switch (result) {
                case CLI_AUTH_OK:
                    /* Authentication passed, level updated by callback */
                    break;
                case CLI_AUTH_DENIED:
                    cli_outputf(cli, "access denied: '%s'\r\n", cmd->cmd);
                    return -EPERM;
                case CLI_AUTH_NEED_LOGIN:
                    cli_outputf(cli, "login required for '%s'\r\n", cmd->cmd);
                    return -EACCES;
                default:
                    return -EINVAL;
            }
        } else {
            /* No authentication callback — deny */
            cli_outputf(cli, "access denied: '%s' (no auth provider)\r\n", cmd->cmd);
            return -EPERM;
        }
    }

    /* Execute the command handler */
    return cmd->fn(argc, (char **)argv);
}

/**
 * @brief Microrl-compatible wrapper for cli_execute_command.
 *
 * microrl calls execute-callback with signature: int(*)(int argc, const char*const* argv).
 * This adapter bridges to the real handler which also needs a cli_t pointer.
 * We use g_current_cli (set in cli_proc) to resolve the instance.
 */
static int cli_execute_adapter(int argc, const char *const *argv)
{
    if (g_current_cli == NULL) return 0;
    return cli_execute_command(g_current_cli, argc, argv);
}

/* Current active CLI instance (exported for ucmd.c callbacks).
 * In a multi-instance setup each proc call sets this briefly. */
cli_t *g_current_cli = NULL;

/* ---------------------------------------------------------------
 * SIGINT (Ctrl+C) handler
 * ---------------------------------------------------------------*/

/**
 * @brief Internal SIGINT handler called from microrl.
 *
 * If a custom sigint_cb is registered, it is called with context
 * about the interrupted command. Otherwise prints a default message.
 */
static void cli_on_sigint(void)
{
    /* This function is called from microrl's KEY_ETX handler.
     * We need access to the cli_t instance. The sigint callback
     * in microrl is void(*)(void) with no context, so we use a
     * static pointer that is set during cli_create. */
    if (g_current_cli != NULL) {
        if (g_current_cli->sigint_cb == NULL) {
            cli_outputf(g_current_cli, "^C\r\n");
            return;
        }

        cli_sigint_info_t info;
        info.interrupted_cmd = g_current_cli->last_command;
        info.argc            = 0;
        info.argv            = NULL;
        info.user_ctx        = g_current_cli->auth_user_ctx;

        g_current_cli->sigint_cb(&info);
    }
}

/* ---------------------------------------------------------------
 * Help command handler (uses current command table)
 * ---------------------------------------------------------------*/

/**
 * @brief Print help for all commands in the current CLI instance.
 * This is called from the global ucmd_help wrapper which resolves
 * the instance via g_current_cli.
 */
static void cli_print_help(const cli_command_t *cmds)
{
    if (g_current_cli == NULL || cmds == NULL) return;

    const cli_command_t *p = cmds;
    while (p->cmd) {
        cli_outputf(g_current_cli, "%s \t%s\r\n", p->cmd, p->help);
        p++;
    }
}

/* ---------------------------------------------------------------
 * Public API implementation
 * ---------------------------------------------------------------*/

/**
 * @brief Create a CLI instance.
 */
cli_t *cli_create(const cli_config_t *config, void *mem, size_t mem_size)
{
    if (!config || !mem || mem_size < sizeof(cli_t)) {
        return NULL;
    }

    /* Zero-fill the memory to ensure clean state */
    memset(mem, 0, sizeof(cli_t));

    cli_t *cli = (cli_t *)mem;

    /* Copy configuration into instance */
    cli->iface          = config->iface;
    cli->commands       = config->commands;
    cli->auth_cb        = config->auth_cb;
    cli->auth_user_ctx  = config->auth_user_ctx;
    cli->sigint_cb      = config->sigint_cb;
    cli->session_priv   = CLI_PRIV_PUBLIC;

    /* Validate required fields */
    if (!cli->iface || !cli->commands) {
        return NULL;
    }

    /* Initialize microrl with our print callback and cli context */
    microrl_init(&cli->rl, cli_print_callback, cli);

    /* Set custom prompt if provided */
    if (config->prompt != NULL) {
        cli->rl.prompt_str = (char *)config->prompt;
    }

    /* Register execute callback (use adapter with microrl-compatible signature) */
    microrl_set_execute_callback(&cli->rl,
                                 (int (*)(int, const char * const *))cli_execute_adapter);

    /* Register SIGINT callback */
    microrl_set_sigint_callback(&cli->rl, cli_on_sigint);

    return cli;
}

/**
 * @brief Destroy a CLI instance (reset internal state).
 */
void cli_destroy(cli_t *cli)
{
    if (!cli) return;

    /* Reset the microrl context */
    memset(cli->rl.cmdline, 0, _COMMAND_LINE_LEN);
    cli->rl.cmdlen = 0;
    cli->rl.cursor = 0;
    cli->last_command = NULL;

    /* Clear session state */
    cli->session_priv = CLI_PRIV_PUBLIC;
    cli->proc_handler = NULL;

    /* If this was the current CLI, clear the pointer */
    if (g_current_cli == cli) {
        g_current_cli = NULL;
    }
}

/**
 * @brief Process one cycle (call from main loop).
 */
void cli_proc(cli_t *cli)
{
    if (!cli || !cli->iface) return;

    /* Set current CLI for callback context */
    g_current_cli = cli;

    /* If a proc handler is installed, delegate to it */
    if (cli->proc_handler != NULL) {
        cli->proc_handler(cli);
        return;
    }

    /* Standard CLI behavior: read one byte and feed to microrl */
    uint8_t ch;
    int ret = cli->iface->read(&ch, 1);
    if (ret <= 0) return;

    microrl_insert_char(&cli->rl, ch);
}

/**
 * @brief Set/clear proc hijack handler.
 */
void cli_set_proc_handler(cli_t *cli, cli_proc_handler_t handler)
{
    if (!cli) return;
    cli->proc_handler = handler;
}

/**
 * @brief Get the drv_face_t interface from an instance.
 */
const drv_face_t *cli_get_iface(const cli_t *cli)
{
    if (!cli) return NULL;
    return cli->iface;
}

/**
 * @brief Output a string through the CLI instance's interface.
 */
void cli_output(cli_t *cli, const char *str)
{
    if (!cli || !cli->iface || !str) return;

    size_t len = 0;
    const char *p = str;
    while (*p && len < CLI_TX_BUF_SIZE) {
        p++;
        len++;
    }
    cli->iface->write(str, len);
}

/**
 * @brief Output a formatted string through the CLI instance's interface.
 */
void cli_outputf(cli_t *cli, const char *fmt, ...)
{
    if (!cli || !cli->iface || !fmt) return;

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(cli->tx_buf, sizeof(cli->tx_buf), fmt, args);
    va_end(args);

    if (len > 0 && len < (int)sizeof(cli->tx_buf)) {
        cli->iface->write(cli->tx_buf, (size_t)len);
    }
}

/**
 * @brief Get the current session privilege level.
 */
cli_privilege_t cli_get_session_privilege(const cli_t *cli)
{
    if (!cli) return CLI_PRIV_PUBLIC;
    return cli->session_priv;
}

/**
 * @brief Set the session privilege level.
 */
void cli_set_session_privilege(cli_t *cli, cli_privilege_t level)
{
    if (!cli) return;
    cli->session_priv = level;
}

/* ---------------------------------------------------------------
 * Backward compatibility wrappers (for existing command modules)
 * ---------------------------------------------------------------*/

/**
 * @brief Compatibility wrapper: ucmd_help now prints help from the
 *        current CLI instance's command table.
 */
int ucmd_help_compat(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    if (g_current_cli && g_current_cli->commands) {
        cli_print_help(g_current_cli->commands);
    }
    return 0;
}

/* ---------------------------------------------------------------
 * Prompt display helper (called after cli_create to show initial prompt)
 * ---------------------------------------------------------------*/

/**
 * @brief Show the initial prompt after CLI creation.
 * Call this after cli_create() to print the first prompt.
 */
void cli_show_prompt(cli_t *cli)
{
    if (!cli) return;
    g_current_cli = cli;
    microrl_insert_char(&cli->rl, '\n');
    microrl_insert_char(&cli->rl, '\n');
}