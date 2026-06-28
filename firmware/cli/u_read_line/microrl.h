
/*
 * Based on microrl library by Eugene Samoylov (Helius) <ghelius@gmail.com>
 * Original: https://github.com/helius/microrl
 * Modified for firmware-demo project by Michael Kaa
 */
#ifndef _MICRORL_H_
#define _MICRORL_H_

#include <stdbool.h>
#include <stdint.h>
#include "config.h"

/*---------------------------------------------------------------
 * Key codes (control characters)
 *---------------------------------------------------------------*/
#define KEY_NUL 0   /**< ^@ Null character */
#define KEY_SOH 1   /**< ^A Start of heading, = console interrupt */
#define KEY_STX 2   /**< ^B Start of text, maintenance mode on HP console */
#define KEY_ETX 3   /**< ^C End of text */
#define KEY_EOT 4   /**< ^D End of transmission, not the same as ETB */
#define KEY_ENQ 5   /**< ^E Enquiry, goes with ACK; old HP flow control */
#define KEY_ACK 6   /**< ^F Acknowledge, clears ENQ logon hand */
#define KEY_BEL 7   /**< ^G Bell, rings the bell... */
#define KEY_BS  8   /**< ^H Backspace, works on HP terminals/computers */
#define KEY_HT  9   /**< ^I Horizontal tab, move to next tab stop */
#define KEY_LF  10  /**< ^J Line Feed */
#define KEY_VT  11  /**< ^K Vertical tab */
#define KEY_FF  12  /**< ^L Form Feed, page eject */
#define KEY_CR  13  /**< ^M Carriage Return*/
#define KEY_SO  14  /**< ^N Shift Out, alternate character set */
#define KEY_SI  15  /**< ^O Shift In, resume defaultn character set */
#define KEY_DLE 16  /**< ^P Data link escape */
#define KEY_DC1 17  /**< ^Q XON, with XOFF to pause listings; "okay to send". */
#define KEY_DC2 18  /**< ^R Device control 2, block-mode flow control */
#define KEY_DC3 19  /**< ^S XOFF, with XON is TERM=18 flow control */
#define KEY_DC4 20  /**< ^T Device control 4 */
#define KEY_NAK 21  /**< ^U Negative acknowledge */
#define KEY_SYN 22  /**< ^V Synchronous idle */
#define KEY_ETB 23  /**< ^W End transmission block, not the same as EOT */
#define KEY_CAN 24  /**< ^X Cancel line, MPE echoes !!! */
#define KEY_EM  25  /**< ^Y End of medium, Control-Y interrupt */
#define KEY_SUB 26  /**< ^Z Substitute */
#define KEY_ESC 27  /**< ^[ Escape, next character is not echoed */
#define KEY_FS  28  /**< ^\ File separator */
#define KEY_GS  29  /**< ^] Group separator */
#define KEY_RS  30  /**< ^^ Record separator, block-mode terminator */
#define KEY_US  31  /**< ^_ Unit separator */

#define KEY_DEL 127 /**< Delete (not a real control character...) */

#define IS_CONTROL_CHAR(x) ((x)<=31)

/*---------------------------------------------------------------
 * History direction
 *---------------------------------------------------------------*/
#define _HIST_UP   0
#define _HIST_DOWN 1

/*---------------------------------------------------------------
 * ESC-sequence internal codes
 *---------------------------------------------------------------*/
#define _ESC_BRACKET  1
#define _ESC_HOME     2
#define _ESC_END      3

#ifdef _USE_HISTORY
/**
 * @struct ring_history_t
 * @brief Static ring buffer for command history.
 *
 * History is stored in a fixed-size ring buffer to avoid dynamic allocation.
 */
typedef struct {
    char ring_buf [_RING_HISTORY_LEN];
    int  begin;
    int  end;
    int  cur;
} ring_history_t;
#endif

/**
 * @struct microrl_t
 * @brief Main microrl context containing all internal state.
 */
typedef struct {
#ifdef _USE_ESC_SEQ
    char     escape_seq;      /**< intermediate ESC-sequence state         */
    char     escape;          /**< non-zero while parsing an ESC sequence  */
    uint32_t escape_stamp;    /**< timestamp of last ESC character         */
#endif
#if (defined(_ENDL_CRLF) || defined(_ENDL_LFCR))
    char tmpch;               /**< helper for CRLF / LFCR detection        */
#endif
#ifdef _USE_HISTORY
    ring_history_t ring_hist; /**< history ring-buffer object             */
#endif
    char *prompt_str;         /**< pointer to prompt string                */
    char cmdline [_COMMAND_LINE_LEN]; /**< command-line buffer            */
    int  cmdlen;              /**< current command-line length             */
    int  cursor;              /**< input cursor position                   */
    int (*execute)(int argc, const char * const *argv);    /**< execute callback      */
    char **(*get_completion)(int argc, const char * const *argv); /**< completion callback */
    void (*print)(const char *, void *ctx);                 /**< print callback        */
    void *print_ctx;                                             /**< context for print */
#ifdef _USE_CTLR_C
    void (*sigint)(void);    /**< Ctrl+C handler                        */
#endif
} microrl_t;

/**
 * @brief Initialize microrl internal data (call once at startup).
 * @param pThis  Pointer to microrl context.
 * @param print  Callback function for outputting strings to terminal.
 * @param ctx    User context passed to the print callback.
 */
void microrl_init(microrl_t *pThis, void (*print)(const char *, void *ctx), void *ctx);

/**
 * @brief Set echo mode (true/false), used for disabling echo (e.g., password).
 * @param echo  1 to enable echo, 0 to disable.
 */
void microrl_set_echo(int);

/**
 * @brief Set completion callback (invoked when user presses Tab).
 *
 * The callback receives argc and argv (tokenized command line) and must
 * return a NULL-terminated array of completion variants separated by
 * whitespace. If a single token is returned it will be auto-completed;
 * an empty string means no completion found; multiple strings indicate
 * ambiguity.
 *
 * @param pThis            Pointer to microrl context.
 * @param get_completion   Completion callback function.
 */
void microrl_set_complete_callback(microrl_t *pThis,
                                   char **(*get_completion)(int, const char * const *));

/**
 * @brief Set execute callback (invoked when user presses Enter).
 *
 * @param pThis     Pointer to microrl context.
 * @param execute   Execute callback function.
 */
void microrl_set_execute_callback(microrl_t *pThis,
                                  int (*execute)(int, const char * const *));

/**
 * @brief Set Ctrl+C (SIGINT) handler callback.
 *
 * @param pThis     Pointer to microrl context.
 * @param sigintf   SIGINT handler function.
 */
#ifdef _USE_CTLR_C
void microrl_set_sigint_callback(microrl_t *pThis, void (*sigintf)(void));
#endif

/**
 * @brief Insert a character into the command line (call from input stream).
 *
 * Typically invoked from UART RX interrupt or polling loop for each
 * received byte.
 *
 * @param pThis  Pointer to microrl context.
 * @param ch     Received character.
 */
void microrl_insert_char(microrl_t *pThis, int ch);

#endif /* _MICRORL_H_ */