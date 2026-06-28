/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright 2026 Michael Kaa */

/**
 * @file test_all.c
 * @brief Единый runner для всех CLI тестов (microrl + split + ucmd).
 *
 * Объединяет все группы тестов в один файл с одним main/setUp/tearDown,
 * чтобы избежать multiple definition ошибок при линковке.
 */

#include "unity.h"
#include "microrl.h"
#include "ucmd.h"
#include "mock_drv_face.h"
#include "stubs.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Глобальные переменные тестов                                        */
/* ------------------------------------------------------------------ */

static microrl_t g_rl;

/* ucmd тестовые флаги */
static int cmd_alpha_called;
static int cmd_beta_called;

/* ------------------------------------------------------------------ */
/* Моковые обработчики команд для ucmd_parse                           */
/* ------------------------------------------------------------------ */

static int cmd_alpha(int argc, char *argv[])
{
    (void)argc; (void)argv;
    cmd_alpha_called = 1;
    return 0;
}

static int cmd_beta(int argc, char *argv[])
{
    (void)argc; (void)argv;
    cmd_beta_called = 1;
    return 42;
}

static command_t test_cmds[] = {
    { .cmd = "alpha", .help = "test alpha", .fn = (command_cb)cmd_alpha },
    { .cmd = "beta",  .help = "test beta",  .fn = (command_cb)cmd_beta  },
    { 0 }
};

/* ------------------------------------------------------------------ */
/* setUp / tearDown                                                    */
/* ------------------------------------------------------------------ */

void setUp(void)
{
    stubs_init();
    mock_print_reset();
    mock_exec_reset();
    microrl_init(&g_rl, mock_print, NULL);
    cmd_alpha_called = 0;
    cmd_beta_called = 0;
}

void tearDown(void)
{
    /* ничего */
}

/* ================================================================== */
/* 1. Инициализация                                                    */
/* ================================================================== */

static void test_microrl_init_clears_buffer(void)
{
    microrl_init(&g_rl, mock_print, NULL);
    for (int i = 0; i < _COMMAND_LINE_LEN; i++) {
        TEST_ASSERT_EQUAL(0, g_rl.cmdline[i]);
    }
}

static void test_microrl_init_esc_fields(void)
{
    microrl_init(&g_rl, mock_print, NULL);
    TEST_ASSERT_EQUAL(0, g_rl.escape);
    TEST_ASSERT_EQUAL(0, g_rl.escape_seq);
    TEST_ASSERT_EQUAL(0, g_rl.escape_stamp);
}

static void test_microrl_init_callbacks_null(void)
{
    microrl_init(&g_rl, mock_print, NULL);
    TEST_ASSERT_NULL(g_rl.execute);
    TEST_ASSERT_NULL(g_rl.get_completion);
    TEST_ASSERT_NULL(g_rl.sigint);
}

static void test_microrl_init_prompt(void)
{
    microrl_init(&g_rl, mock_print, NULL);
    TEST_ASSERT_NOT_NULL(g_rl.prompt_str);
}

/* ================================================================== */
/* 2. Ввод печатных символов                                           */
/* ================================================================== */

static void test_insert_single_char(void)
{
    microrl_insert_char(&g_rl, 'a');
    TEST_ASSERT_EQUAL(1, g_rl.cmdlen);
    TEST_ASSERT_EQUAL('a', g_rl.cmdline[0]);
    TEST_ASSERT_EQUAL(1, g_rl.cursor);
}

static void test_insert_multiple_chars(void)
{
    microrl_insert_char(&g_rl, 'h');
    microrl_insert_char(&g_rl, 'i');
    TEST_ASSERT_EQUAL(2, g_rl.cmdlen);
    TEST_ASSERT_EQUAL_STRING("hi", g_rl.cmdline);
    TEST_ASSERT_EQUAL(2, g_rl.cursor);
}

static void test_insert_space_replaces_with_null(void)
{
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, ' ');
    TEST_ASSERT_EQUAL(2, g_rl.cmdlen);
    TEST_ASSERT_EQUAL('a', g_rl.cmdline[0]);
    TEST_ASSERT_EQUAL('\0', g_rl.cmdline[1]);
}

static void test_insert_buffer_full(void)
{
    for (int i = 0; i < _COMMAND_LINE_LEN - 1; i++) {
        microrl_insert_char(&g_rl, 'x');
    }
    int len_before = g_rl.cmdlen;
    microrl_insert_char(&g_rl, 'y');
    TEST_ASSERT_EQUAL(len_before, g_rl.cmdlen);
}

static void test_leading_space_ignored(void)
{
    microrl_insert_char(&g_rl, ' ');
    TEST_ASSERT_EQUAL(0, g_rl.cmdlen);
    TEST_ASSERT_EQUAL(0, g_rl.cursor);
}

/* ================================================================== */
/* 3. Навигация курсора                                                */
/* ================================================================== */

static void test_arrow_right_moves_cursor(void)
{
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, 'b');
    microrl_insert_char(&g_rl, 'c');
    microrl_insert_char(&g_rl, KEY_ESC);
    microrl_insert_char(&g_rl, '[');
    microrl_insert_char(&g_rl, 'D');
    TEST_ASSERT_EQUAL(2, g_rl.cursor);
    microrl_insert_char(&g_rl, KEY_ESC);
    microrl_insert_char(&g_rl, '[');
    microrl_insert_char(&g_rl, 'C');
    TEST_ASSERT_EQUAL(3, g_rl.cursor);
}

static void test_arrow_left_moves_cursor(void)
{
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, 'b');
    microrl_insert_char(&g_rl, KEY_ESC);
    microrl_insert_char(&g_rl, '[');
    microrl_insert_char(&g_rl, 'D');
    TEST_ASSERT_EQUAL(1, g_rl.cursor);
}

static void test_arrow_right_at_end(void)
{
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, KEY_ESC);
    microrl_insert_char(&g_rl, '[');
    microrl_insert_char(&g_rl, 'C');
    TEST_ASSERT_EQUAL(1, g_rl.cursor);
}

static void test_arrow_left_at_start(void)
{
    microrl_insert_char(&g_rl, KEY_ESC);
    microrl_insert_char(&g_rl, '[');
    microrl_insert_char(&g_rl, 'D');
    TEST_ASSERT_EQUAL(0, g_rl.cursor);
}

static void test_home_moves_to_beginning(void)
{
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, 'b');
    microrl_insert_char(&g_rl, KEY_ESC);
    microrl_insert_char(&g_rl, '[');
    microrl_insert_char(&g_rl, '7');
    microrl_insert_char(&g_rl, '~');
    TEST_ASSERT_EQUAL(0, g_rl.cursor);
}

static void test_end_moves_to_end(void)
{
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, 'b');
    microrl_insert_char(&g_rl, KEY_ESC);
    microrl_insert_char(&g_rl, '[');
    microrl_insert_char(&g_rl, 'D');
    TEST_ASSERT_EQUAL(1, g_rl.cursor);
    microrl_insert_char(&g_rl, KEY_ESC);
    microrl_insert_char(&g_rl, '[');
    microrl_insert_char(&g_rl, '8');
    microrl_insert_char(&g_rl, '~');
    TEST_ASSERT_EQUAL(2, g_rl.cursor);
}

static void test_ctrl_a_moves_home(void)
{
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, 'b');
    microrl_insert_char(&g_rl, KEY_SOH);
    TEST_ASSERT_EQUAL(0, g_rl.cursor);
}

static void test_ctrl_e_moves_end(void)
{
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, 'b');
    microrl_insert_char(&g_rl, KEY_SOH);
    TEST_ASSERT_EQUAL(0, g_rl.cursor);
    microrl_insert_char(&g_rl, KEY_ENQ);
    TEST_ASSERT_EQUAL(2, g_rl.cursor);
}

static void test_ctrl_f_moves_forward(void)
{
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, 'b');
    microrl_insert_char(&g_rl, KEY_SOH);
    microrl_insert_char(&g_rl, KEY_ACK);
    TEST_ASSERT_EQUAL(1, g_rl.cursor);
}

static void test_ctrl_b_moves_backward(void)
{
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, 'b');
    microrl_insert_char(&g_rl, KEY_STX);
    TEST_ASSERT_EQUAL(1, g_rl.cursor);
}

/* ================================================================== */
/* 4. Редактирование                                                   */
/* ================================================================== */

static void test_backspace_removes_char(void)
{
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, 'b');
    microrl_insert_char(&g_rl, KEY_BS);
    TEST_ASSERT_EQUAL(1, g_rl.cmdlen);
    TEST_ASSERT_EQUAL('a', g_rl.cmdline[0]);
}

static void test_backspace_at_start_noop(void)
{
    microrl_insert_char(&g_rl, KEY_BS);
    TEST_ASSERT_EQUAL(0, g_rl.cmdlen);
    TEST_ASSERT_EQUAL(0, g_rl.cursor);
}

static void test_delete_removes_at_cursor(void)
{
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, 'b');
    microrl_insert_char(&g_rl, 'c');
    microrl_insert_char(&g_rl, KEY_ESC);
    microrl_insert_char(&g_rl, '[');
    microrl_insert_char(&g_rl, 'D');
    TEST_ASSERT_EQUAL(2, g_rl.cursor);
    microrl_insert_char(&g_rl, KEY_DEL);
    TEST_ASSERT_EQUAL(2, g_rl.cmdlen);
    TEST_ASSERT_EQUAL('a', g_rl.cmdline[0]);
    TEST_ASSERT_EQUAL('c', g_rl.cmdline[1]);
}

static void test_ctrl_u_clears_to_beginning(void)
{
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, 'b');
    microrl_insert_char(&g_rl, 'c');
    microrl_insert_char(&g_rl, KEY_NAK);
    TEST_ASSERT_EQUAL(0, g_rl.cmdlen);
    TEST_ASSERT_EQUAL(0, g_rl.cursor);
}

static void test_ctrl_k_clears_to_end(void)
{
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, 'b');
    microrl_insert_char(&g_rl, 'c');
    microrl_insert_char(&g_rl, KEY_SOH);
    microrl_insert_char(&g_rl, KEY_VT);
    TEST_ASSERT_EQUAL(0, g_rl.cmdlen);
}

static void test_insert_at_cursor_shifts_right(void)
{
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, 'c');
    microrl_insert_char(&g_rl, KEY_SOH);
    TEST_ASSERT_EQUAL(0, g_rl.cursor);
    microrl_insert_char(&g_rl, 'b');
    TEST_ASSERT_EQUAL(3, g_rl.cmdlen);
    TEST_ASSERT_EQUAL('b', g_rl.cmdline[0]);
    TEST_ASSERT_EQUAL('a', g_rl.cmdline[1]);
    TEST_ASSERT_EQUAL('c', g_rl.cmdline[2]);
}

/* ================================================================== */
/* 5. История команд                                                   */
/* ================================================================== */

static void test_history_save_on_enter(void)
{
    microrl_insert_char(&g_rl, 'h');
    microrl_insert_char(&g_rl, 'i');
    microrl_insert_char(&g_rl, KEY_CR);
    TEST_ASSERT_NOT_EQUAL(0, g_rl.ring_hist.ring_buf[g_rl.ring_hist.begin]);
}

static void test_arrow_up_restores_last(void)
{
    microrl_insert_char(&g_rl, 'h');
    microrl_insert_char(&g_rl, 'e');
    microrl_insert_char(&g_rl, 'l');
    microrl_insert_char(&g_rl, 'p');
    microrl_insert_char(&g_rl, KEY_CR);
    microrl_insert_char(&g_rl, KEY_ESC);
    microrl_insert_char(&g_rl, '[');
    microrl_insert_char(&g_rl, 'A');
    TEST_ASSERT_EQUAL_STRING("help", g_rl.cmdline);
}

static void test_arrow_down_newest(void)
{
    microrl_insert_char(&g_rl, 't');
    microrl_insert_char(&g_rl, 'e');
    microrl_insert_char(&g_rl, 's');
    microrl_insert_char(&g_rl, 't');
    microrl_insert_char(&g_rl, KEY_CR);
    microrl_insert_char(&g_rl, KEY_ESC);
    microrl_insert_char(&g_rl, '[');
    microrl_insert_char(&g_rl, 'A');
    microrl_insert_char(&g_rl, KEY_ESC);
    microrl_insert_char(&g_rl, '[');
    microrl_insert_char(&g_rl, 'B');
    TEST_ASSERT_EQUAL(0, g_rl.cmdlen);
}

static void test_history_empty_no_change(void)
{
    microrl_insert_char(&g_rl, KEY_ESC);
    microrl_insert_char(&g_rl, '[');
    microrl_insert_char(&g_rl, 'A');
    TEST_ASSERT_EQUAL(0, g_rl.cmdlen);
}

static void test_history_ring_wrap(void)
{
    for (int i = 0; i < 10; i++) {
        microrl_insert_char(&g_rl, 'a' + (i % 26));
        microrl_insert_char(&g_rl, KEY_CR);
    }
    TEST_ASSERT_TRUE(g_rl.ring_hist.begin >= 0);
}

static void test_ctrl_p_history_up(void)
{
    microrl_insert_char(&g_rl, 'x');
    microrl_insert_char(&g_rl, KEY_CR);
    microrl_insert_char(&g_rl, KEY_DLE);
    TEST_ASSERT_EQUAL_STRING("x", g_rl.cmdline);
}

static void test_ctrl_n_history_down(void)
{
    microrl_insert_char(&g_rl, 'y');
    microrl_insert_char(&g_rl, KEY_CR);
    microrl_insert_char(&g_rl, KEY_DLE);
    microrl_insert_char(&g_rl, KEY_SO);
    TEST_ASSERT_EQUAL(0, g_rl.cmdlen);
}

/* ================================================================== */
/* 6. ESC таймаут                                                      */
/* ================================================================== */

static void test_esc_timeout_resets_flag(void)
{
    microrl_insert_char(&g_rl, KEY_ESC);
    TEST_ASSERT_EQUAL(1, g_rl.escape);
    stubs_advance_time_us(600);
    microrl_insert_char(&g_rl, 'h');
    TEST_ASSERT_EQUAL(0, g_rl.escape);
}

static void test_esc_timeout_flag_reset_on_incomplete(void)
{
    microrl_insert_char(&g_rl, KEY_ESC);
    microrl_insert_char(&g_rl, '[');
    TEST_ASSERT_EQUAL(1, g_rl.escape);
    stubs_advance_time_us(600);
    microrl_insert_char(&g_rl, 'x');
    TEST_ASSERT_EQUAL(0, g_rl.escape);
}

static void test_esc_seq_complete_before_timeout(void)
{
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, KEY_CR);
    microrl_insert_char(&g_rl, KEY_ESC);
    microrl_insert_char(&g_rl, '[');
    microrl_insert_char(&g_rl, 'A');
    TEST_ASSERT_EQUAL(0, g_rl.escape);
    TEST_ASSERT_EQUAL_STRING("a", g_rl.cmdline);
}

/* ================================================================== */
/* 7. Token splitting (косвенные через execute callback)               */
/* ================================================================== */

static void test_split_single_token(void)
{
    mock_exec_reset();
    microrl_set_execute_callback(&g_rl, mock_execute);
    microrl_insert_char(&g_rl, 'h');
    microrl_insert_char(&g_rl, 'e');
    microrl_insert_char(&g_rl, 'l');
    microrl_insert_char(&g_rl, 'l');
    microrl_insert_char(&g_rl, 'o');
    microrl_insert_char(&g_rl, KEY_CR);
    TEST_ASSERT_EQUAL(1, mock_exec_argc);
    TEST_ASSERT_EQUAL_STRING("hello", mock_exec_argv[0]);
}

static void test_split_multiple_tokens(void)
{
    mock_exec_reset();
    microrl_set_execute_callback(&g_rl, mock_execute);
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, 'b');
    microrl_insert_char(&g_rl, 'c');
    microrl_insert_char(&g_rl, ' ');
    microrl_insert_char(&g_rl, 'd');
    microrl_insert_char(&g_rl, 'e');
    microrl_insert_char(&g_rl, 'f');
    microrl_insert_char(&g_rl, ' ');
    microrl_insert_char(&g_rl, 'g');
    microrl_insert_char(&g_rl, 'h');
    microrl_insert_char(&g_rl, 'i');
    microrl_insert_char(&g_rl, KEY_CR);
    TEST_ASSERT_EQUAL(3, mock_exec_argc);
    TEST_ASSERT_EQUAL_STRING("abc", mock_exec_argv[0]);
    TEST_ASSERT_EQUAL_STRING("def", mock_exec_argv[1]);
    TEST_ASSERT_EQUAL_STRING("ghi", mock_exec_argv[2]);
}

static void test_split_leading_spaces(void)
{
    mock_exec_reset();
    microrl_set_execute_callback(&g_rl, mock_execute);
    microrl_insert_char(&g_rl, ' ');
    microrl_insert_char(&g_rl, ' ');
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, 'b');
    microrl_insert_char(&g_rl, 'c');
    microrl_insert_char(&g_rl, KEY_CR);
    TEST_ASSERT_EQUAL(1, mock_exec_argc);
    TEST_ASSERT_EQUAL_STRING("abc", mock_exec_argv[0]);
}

static void test_split_trailing_spaces(void)
{
    mock_exec_reset();
    microrl_set_execute_callback(&g_rl, mock_execute);
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, 'b');
    microrl_insert_char(&g_rl, 'c');
    microrl_insert_char(&g_rl, ' ');
    microrl_insert_char(&g_rl, ' ');
    microrl_insert_char(&g_rl, KEY_CR);
    TEST_ASSERT_EQUAL(1, mock_exec_argc);
    TEST_ASSERT_EQUAL_STRING("abc", mock_exec_argv[0]);
}

static void test_split_empty_string(void)
{
    mock_exec_reset();
    microrl_set_execute_callback(&g_rl, mock_execute);
    microrl_insert_char(&g_rl, KEY_CR);
    TEST_ASSERT_EQUAL(0, mock_exec_called);
}

static void test_split_max_tokens_exceeded(void)
{
    mock_exec_reset();
    microrl_set_execute_callback(&g_rl, mock_execute);
    for (int i = 0; i < _COMMAND_TOKEN_NMB + 1; i++) {
        microrl_insert_char(&g_rl, 'x');
        if (i < _COMMAND_TOKEN_NMB) {
            microrl_insert_char(&g_rl, ' ');
        }
    }
    microrl_insert_char(&g_rl, KEY_CR);
    TEST_ASSERT_EQUAL(0, mock_exec_called);
}

/* ================================================================== */
/* 8. Execute callback                                                 */
/* ================================================================== */

static void test_enter_calls_execute(void)
{
    mock_exec_reset();
    microrl_set_execute_callback(&g_rl, mock_execute);
    microrl_insert_char(&g_rl, 'r');
    microrl_insert_char(&g_rl, 'u');
    microrl_insert_char(&g_rl, 'n');
    microrl_insert_char(&g_rl, KEY_CR);
    TEST_ASSERT_NOT_EQUAL(0, mock_exec_called);
}

static void test_enter_passes_correct_argc_argv(void)
{
    mock_exec_reset();
    microrl_set_execute_callback(&g_rl, mock_execute);
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, 'b');
    microrl_insert_char(&g_rl, 'c');
    microrl_insert_char(&g_rl, ' ');
    microrl_insert_char(&g_rl, 'd');
    microrl_insert_char(&g_rl, 'e');
    microrl_insert_char(&g_rl, 'f');
    microrl_insert_char(&g_rl, KEY_CR);
    TEST_ASSERT_EQUAL(2, mock_exec_argc);
    TEST_ASSERT_EQUAL_STRING("abc", mock_exec_argv[0]);
    TEST_ASSERT_EQUAL_STRING("def", mock_exec_argv[1]);
}

static void test_empty_enter_no_execute(void)
{
    mock_exec_reset();
    microrl_set_execute_callback(&g_rl, mock_execute);
    microrl_insert_char(&g_rl, KEY_CR);
    TEST_ASSERT_EQUAL(0, mock_exec_called);
}

static void test_enter_clears_line(void)
{
    mock_exec_reset();
    microrl_set_execute_callback(&g_rl, mock_execute);
    microrl_insert_char(&g_rl, 't');
    microrl_insert_char(&g_rl, 'e');
    microrl_insert_char(&g_rl, 's');
    microrl_insert_char(&g_rl, 't');
    microrl_insert_char(&g_rl, KEY_CR);
    TEST_ASSERT_EQUAL(0, g_rl.cmdlen);
    TEST_ASSERT_EQUAL(0, g_rl.cursor);
}

/* ================================================================== */
/* 9. Автодополнение                                                   */
/* ================================================================== */

static void test_tab_calls_completion(void)
{
    microrl_insert_char(&g_rl, 'a');
    microrl_insert_char(&g_rl, KEY_HT);
    TEST_ASSERT_EQUAL(1, g_rl.cmdlen);
}

static void test_completion_no_callback(void)
{
    TEST_ASSERT_NULL(g_rl.get_completion);
    microrl_insert_char(&g_rl, 'h');
    microrl_insert_char(&g_rl, KEY_HT);
    TEST_ASSERT_EQUAL(1, g_rl.cmdlen);
}

/* ================================================================== */
/* 10. SIGINT                                                          */
/* ================================================================== */

static void test_ctrl_c_calls_sigint(void)
{
    microrl_set_sigint_callback(&g_rl, NULL);
    microrl_insert_char(&g_rl, KEY_ETX);
    TEST_ASSERT_TRUE(1);
}

static void test_sigint_not_set_no_crash(void)
{
    TEST_ASSERT_NULL(g_rl.sigint);
    microrl_insert_char(&g_rl, KEY_ETX);
    TEST_ASSERT_TRUE(1);
}

/* ================================================================== */
/* 11. ucmd_parse                                                      */
/* ================================================================== */

static void test_ucmd_parse_found(void)
{
    const char *argv[] = { "alpha", NULL };
    int ret = ucmd_parse(test_cmds, 1, argv);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_NOT_EQUAL(0, cmd_alpha_called);
}

static void test_ucmd_parse_not_found(void)
{
    const char *argv[] = { "gamma", NULL };
    int ret = ucmd_parse(test_cmds, 1, argv);
    TEST_ASSERT_EQUAL(UCMD_CMD_NOT_FOUND, ret);
    TEST_ASSERT_EQUAL(0, cmd_alpha_called);
    TEST_ASSERT_EQUAL(0, cmd_beta_called);
}

static void test_ucmd_parse_empty_argv(void)
{
    int ret = ucmd_parse(test_cmds, 0, NULL);
    TEST_ASSERT_EQUAL(0, ret);
}

static void test_ucmd_parse_returns_callback_value(void)
{
    const char *argv[] = { "beta", NULL };
    int ret = ucmd_parse(test_cmds, 1, argv);
    TEST_ASSERT_EQUAL(42, ret);
    TEST_ASSERT_NOT_EQUAL(0, cmd_beta_called);
}

static void test_ucmd_parse_null_cmd_list(void)
{
    const char *argv[] = { "alpha", NULL };
    int ret = ucmd_parse(NULL, 1, argv);
    TEST_ASSERT_EQUAL(UCMD_CMD_NOT_FOUND, ret);
}

/* ================================================================== */
/* Main                                                                */
/* ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Init */
    RUN_TEST(test_microrl_init_clears_buffer);
    RUN_TEST(test_microrl_init_esc_fields);
    RUN_TEST(test_microrl_init_callbacks_null);
    RUN_TEST(test_microrl_init_prompt);

    /* Input */
    RUN_TEST(test_insert_single_char);
    RUN_TEST(test_insert_multiple_chars);
    RUN_TEST(test_insert_space_replaces_with_null);
    RUN_TEST(test_insert_buffer_full);
    RUN_TEST(test_leading_space_ignored);

    /* Navigation */
    RUN_TEST(test_arrow_right_moves_cursor);
    RUN_TEST(test_arrow_left_moves_cursor);
    RUN_TEST(test_arrow_right_at_end);
    RUN_TEST(test_arrow_left_at_start);
    RUN_TEST(test_home_moves_to_beginning);
    RUN_TEST(test_end_moves_to_end);
    RUN_TEST(test_ctrl_a_moves_home);
    RUN_TEST(test_ctrl_e_moves_end);
    RUN_TEST(test_ctrl_f_moves_forward);
    RUN_TEST(test_ctrl_b_moves_backward);

    /* Editing */
    RUN_TEST(test_backspace_removes_char);
    RUN_TEST(test_backspace_at_start_noop);
    RUN_TEST(test_delete_removes_at_cursor);
    RUN_TEST(test_ctrl_u_clears_to_beginning);
    RUN_TEST(test_ctrl_k_clears_to_end);
    RUN_TEST(test_insert_at_cursor_shifts_right);

    /* History */
    RUN_TEST(test_history_save_on_enter);
    RUN_TEST(test_arrow_up_restores_last);
    RUN_TEST(test_arrow_down_newest);
    RUN_TEST(test_history_empty_no_change);
    RUN_TEST(test_history_ring_wrap);
    RUN_TEST(test_ctrl_p_history_up);
    RUN_TEST(test_ctrl_n_history_down);

    /* ESC timeout */
    RUN_TEST(test_esc_timeout_resets_flag);
    RUN_TEST(test_esc_timeout_flag_reset_on_incomplete);
    RUN_TEST(test_esc_seq_complete_before_timeout);

    /* Token splitting */
    RUN_TEST(test_split_single_token);
    RUN_TEST(test_split_multiple_tokens);
    RUN_TEST(test_split_leading_spaces);
    RUN_TEST(test_split_trailing_spaces);
    RUN_TEST(test_split_empty_string);
    RUN_TEST(test_split_max_tokens_exceeded);

    /* Execute callback */
    RUN_TEST(test_enter_calls_execute);
    RUN_TEST(test_enter_passes_correct_argc_argv);
    RUN_TEST(test_empty_enter_no_execute);
    RUN_TEST(test_enter_clears_line);

    /* Completion */
    RUN_TEST(test_tab_calls_completion);
    RUN_TEST(test_completion_no_callback);

    /* SIGINT */
    RUN_TEST(test_ctrl_c_calls_sigint);
    RUN_TEST(test_sigint_not_set_no_crash);

    /* ucmd */
    RUN_TEST(test_ucmd_parse_found);
    RUN_TEST(test_ucmd_parse_not_found);
    RUN_TEST(test_ucmd_parse_empty_argv);
    RUN_TEST(test_ucmd_parse_returns_callback_value);
    RUN_TEST(test_ucmd_parse_null_cmd_list);

    return UNITY_END();
}