/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright 2026 Michael Kaa */

/**
 * @file mock_drv_face.c
 * @brief Реализация мокового drv_face_t для unit-тестов u_read_line.
 */

#include "mock_drv_face.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Ввод (mock_read)                                                    */
/* ------------------------------------------------------------------ */

static uint8_t mock_rx_buf[512];
static size_t  mock_rx_len;
static size_t  mock_rx_pos;

static int mock_read_impl(void *buf, size_t len)
{
    (void)len;
    if (mock_rx_pos >= mock_rx_len) {
        return 0; /* нет данных */
    }
    uint8_t *out = (uint8_t *)buf;
    out[0] = mock_rx_buf[mock_rx_pos++];
    return 1;
}

static const drv_face_t s_mock_iface = {
    .read  = mock_read_impl,
    .write = NULL,
    .ioctl = NULL
};

void mock_iface_set_input(const uint8_t *data, size_t len)
{
    if (len > sizeof(mock_rx_buf)) {
        len = sizeof(mock_rx_buf);
    }
    memcpy(mock_rx_buf, data, len);
    mock_rx_len = len;
    mock_rx_pos = 0;
}

void mock_iface_reset(void)
{
    mock_rx_len = 0;
    mock_rx_pos = 0;
}

const drv_face_t *mock_iface_get(void)
{
    return &s_mock_iface;
}

/* ------------------------------------------------------------------ */
/* Вывод (print callback mock)                                         */
/* ------------------------------------------------------------------ */

char    mock_print_buffer[4096];
size_t  mock_print_len;

void mock_print(const char *str, void *ctx)
{
    (void)ctx;
    if (!str) return;
    size_t len = strlen(str);
    if (mock_print_len + len < sizeof(mock_print_buffer)) {
        strcpy(mock_print_buffer + mock_print_len, str);
        mock_print_len += len;
    }
}

void mock_print_reset(void)
{
    mock_print_len = 0;
    mock_print_buffer[0] = '\0';
}

/* ------------------------------------------------------------------ */
/* Execute callback mock (статические буферы, без malloc)               */
/* ------------------------------------------------------------------ */

static char mock_arg_bufs[16][128];

int   mock_exec_called;
int   mock_exec_argc;
char *mock_exec_argv[16];

int mock_execute(int argc, const char * const *argv)
{
    mock_exec_called = 1;
    mock_exec_argc = argc;

    for (int i = 0; i < 16; i++) {
        mock_exec_argv[i] = NULL;
        mock_arg_bufs[i][0] = '\0';
    }

    int n = argc < 16 ? argc : 16;
    for (int i = 0; i < n; i++) {
        if (argv[i]) {
            strncpy(mock_arg_bufs[i], argv[i], sizeof(mock_arg_bufs[i]) - 1);
            mock_arg_bufs[i][sizeof(mock_arg_bufs[i]) - 1] = '\0';
            mock_exec_argv[i] = mock_arg_bufs[i];
        }
    }

    return 0;
}

void mock_exec_reset(void)
{
    mock_exec_called = 0;
    mock_exec_argc = 0;
    for (int i = 0; i < 16; i++) {
        mock_exec_argv[i] = NULL;
        mock_arg_bufs[i][0] = '\0';
    }
}