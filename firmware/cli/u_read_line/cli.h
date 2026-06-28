/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright 2026 Michael Kaa */

#ifndef _CLI_H_
#define _CLI_H_

#include <stddef.h>
#include <stdint.h>
#include "ucmd.h"       /* command_t, command_cb */
#include "drv_face.h"   /* drv_face_t */

/* ---------------------------------------------------------------
 * Privilege levels for commands
 * ---------------------------------------------------------------*/
typedef enum {
    CLI_PRIV_PUBLIC   = 0,    /**< available to everyone          */
    CLI_PRIV_OPERATOR = 1,    /**< requires operator authorization */
    CLI_PRIV_ADMIN    = 2,    /**< requires admin authorization    */
    CLI_PRIV_DISABLED = 0xFF  /**< command disabled (not executed) */
} cli_privilege_t;

/* ---------------------------------------------------------------
 * Extended command structure (with privilege level)
 * ---------------------------------------------------------------*/
typedef struct {
    const char        *cmd;              /**< command string               */
    const char        *help;             /**< help text                    */
    command_cb         fn;               /**< handler function             */
    cli_privilege_t    privilege_level;  /**< required privilege level     */
} cli_command_t;

/* ---------------------------------------------------------------
 * Authentication check result
 * ---------------------------------------------------------------*/
typedef enum {
    CLI_AUTH_OK         =  0, /**< command allowed       */
    CLI_AUTH_DENIED    = -1,  /**< command denied        */
    CLI_AUTH_NEED_LOGIN= -2,  /**< login required        */
} cli_auth_result_t;

/* ---------------------------------------------------------------
 * Callback for authentication check before command execution
 * ---------------------------------------------------------------*/
typedef cli_auth_result_t (*cli_auth_callback_t)(
    const char *command,
    cli_privilege_t required_level,
    void *user_ctx);

/* ---------------------------------------------------------------
 * Callback for proc hijacking. Allows an application to take over
 * the processing loop (interactive mode). On exit the application
 * must set handler = NULL to return control to standard CLI.
 * ---------------------------------------------------------------*/
typedef void (*cli_proc_handler_t)(void *cli_ptr);

/* ---------------------------------------------------------------
 * Information about Ctrl+C interrupt
 * ---------------------------------------------------------------*/
typedef struct {
    const char      *interrupted_cmd; /**< command that was interrupted  */
    int              argc;            /**< argument count                */
    const char     **argv;           /**< argument vector               */
    void           *user_ctx;        /**< application context           */
} cli_sigint_info_t;

typedef void (*cli_sigint_callback_t)(cli_sigint_info_t *info);

/* ---------------------------------------------------------------
 * CLI instance configuration
 * ---------------------------------------------------------------*/
typedef struct {
    const drv_face_t    *iface;         /**< I/O interface              */
    const cli_command_t *commands;      /**< external command table     */
    const char          *prompt;        /**< prompt string (NULL=default) */
    cli_auth_callback_t  auth_cb;       /**< authentication hook (NULL=skip) */
    void               *auth_user_ctx;  /**< context for auth_cb        */
    cli_sigint_callback_t sigint_cb;    /**< Ctrl+C handler             */
} cli_config_t;

/* ---------------------------------------------------------------
 * CLI instance (opaque forward declaration)
 * ---------------------------------------------------------------*/
typedef struct cli cli_t;

/* ---------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------*/

/**
 * @brief Create a CLI instance.
 * @param config  Configuration (lives until cli_destroy or is copied).
 * @param mem     Memory for the instance (allocated by app, no malloc).
 * @param mem_size Size of allocated memory.
 * @return Pointer to the instance or NULL on error.
 */
cli_t *cli_create(const cli_config_t *config, void *mem, size_t mem_size);

/**
 * @brief Destroy a CLI instance.
 */
void cli_destroy(cli_t *cli);

/**
 * @brief Process one cycle (call from main loop).
 * Reads data from iface, processes input, executes commands.
 */
void cli_proc(cli_t *cli);

/**
 * @brief Set/clear proc hijack handler.
 * @param handler  NULL = restore standard CLI behavior.
 */
void cli_set_proc_handler(cli_t *cli, cli_proc_handler_t handler);

/**
 * @brief Get the drv_face_t interface from an instance
 *        (for use by interactive applications).
 */
const drv_face_t *cli_get_iface(const cli_t *cli);

/**
 * @brief Output a string through the CLI instance's interface.
 */
void cli_output(cli_t *cli, const char *str);

/**
 * @brief Output a formatted string through the CLI instance's interface.
 */
void cli_outputf(cli_t *cli, const char *fmt, ...);

/**
 * @brief Get the current session privilege level.
 */
cli_privilege_t cli_get_session_privilege(const cli_t *cli);

/**
 * @brief Set the session privilege level (called from authentication code).
 */
void cli_set_session_privilege(cli_t *cli, cli_privilege_t level);

/**
 * @brief Show the initial prompt after CLI creation.
 * Call this after cli_create() to print the first prompt.
 */
void cli_show_prompt(cli_t *cli);

/* Current active CLI instance (for callback context in microrl).
 * Set by cli_proc() before processing, cleared on destroy. */
extern cli_t *g_current_cli;

#endif /* _CLI_H_ */
