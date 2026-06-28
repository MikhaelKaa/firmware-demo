/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright 2026 Michael Kaa */

/**
 * @file mock_drv_face.h
 * @brief Моковый drv_face_t для unit-тестов модуля u_read_line.
 *
 * Позволяет подставлять входные данные байт за байтом и захватывать
 * вывод (print callback) для проверки echo и сообщений.
 */

#ifndef MOCK_DRV_FACE_H
#define MOCK_DRV_FACE_H

#include <stddef.h>
#include <stdint.h>
#include "drv_face.h"

/* ------------------------------------------------------------------ */
/* Ввод (mock_read)                                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief Установить буфер входных данных для мокового read().
 * @param data  Указатель на массив байт.
 * @param len   Количество байт в буфере.
 */
void mock_iface_set_input(const uint8_t *data, size_t len);

/**
 * @brief Сбросить состояние мокового интерфейса.
 */
void mock_iface_reset(void);

/**
 * @brief Получить указатель на моковый drv_face_t.
 */
const drv_face_t *mock_iface_get(void);

/* ------------------------------------------------------------------ */
/* Вывод (print callback mock)                                         */
/* ------------------------------------------------------------------ */

/** Буфер захвата вывода microrl. */
extern char mock_print_buffer[4096];
/** Текущая длина данных в буфере. */
extern size_t mock_print_len;

/**
 * @brief Моковая функция print для microrl.
 * @param str  Нуль-терминированная строка для записи.
 */
void mock_print(const char *str);

/**
 * @brief Сбросить буфер захвата вывода.
 */
void mock_print_reset(void);

/* ------------------------------------------------------------------ */
/* Execute callback mock                                               */
/* ------------------------------------------------------------------ */

/** Флаг: был ли вызван execute. */
extern int mock_exec_called;
/** Количество аргументов последнего вызова. */
extern int mock_exec_argc;
/** Массив аргументов (статические буферы, без malloc). */
extern char *mock_exec_argv[16];

/**
 * @brief Моковая функция execute для microrl.
 * @param argc  Количество аргументов.
 * @param argv  Массив строк.
 * @return 0 (успех).
 */
int mock_execute(int argc, const char * const *argv);

/**
 * @brief Сбросить состояние мокового execute.
 */
void mock_exec_reset(void);

#endif /* MOCK_DRV_FACE_H */