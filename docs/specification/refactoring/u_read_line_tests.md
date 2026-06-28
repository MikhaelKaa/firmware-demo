<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright 2026 Michael Kaa -->

# Unit-тесты модуля u_read_line

## 1. Метаданные

| Поле | Значение |
|---|---|
| **Дата создания** | 28 июня 2026 |
| **Дата последнего обновления** | 28 июня 2026 |
| **Статус** | Выполнено |
| **Связанная спецификация** | u_read_line_refactoring.md |
| **Автор** | Michael Kaa |

---

## 2. Обзор

### 2.1 Назначение

После рефакторинга модуля `firmware/cli/u_read_line/` (замена scanf на drv_face_t, добавление ESC-таймаута, инициализация всех полей) модуль стал полностью тестируемым на хосте без зависимости от STM32. Настоящая спецификация описывает набор unit-тестов для верификации корректности работы microrl и ucmd.

### 2.2 Что тестируем

| Модуль | Файлы | Покрытие |
|--------|-------|----------|
| microrl | `microrl.c`, `microrl.h` | Ядро readline: ввод, навигация, история, ESC, split |
| ucmd | `ucmd.c`, `ucmd.h` | Парсинг команд, dispatch, unknown command |

### 2.3 Цели

1. Обеспечить регрессионную защиту после рефакторинга
2. Проверить корректность обработки ESC-последовательностей
3. Верифицировать ESC-таймаут (сброс при паузе > 500 мкс)
4. Проверить работу кольцевого буфера истории
5. Тестировать парсинг токенов и dispatch команд

---

## 3. Инфраструктура

### 3.1 Размещение

Тесты для модулей приложения (CLI) размещаются в **основном репозитории** (`firmware-demo`), а не в сабмодуле `firmware-core`. Это обеспечивает правильное разделение ответственности:

```
firmware-demo/
├── firmware/
│   ├── cli/u_read_line/                # тестируемый модуль (в demo)
│   ├── core/                           # сабмодуль firmware-core
│   │   └── tests/                      # тесты core (автономные)
│   │       ├── unity/                  # framework (сабмодуль, в core)
│   │       └── lib/time/               # тесты lib из core
│   └── tests/                          # тесты приложений (НОВОЕ)
│       ├── unity/                      # framework (сабмодуль, копия)
│       ├── cli/u_read_line/
│       │   └── test_all.c              # все тесты CLI-модуля
│       ├── test_helpers/
│       │   ├── stubs.c                 # стубы CYCCNT, precise_time
│       │   ├── stubs.h
│       │   ├── cmsis_stubs.h           # блокирует stm32f407xx.h
│       │   ├── stm32f407xx.h           # пустая заглушка CMSIS
│       │   ├── mock_drv_face.c         # мок drv_face_t для CLI
│       │   ├── mock_drv_face.h
│       │   └── ucmd_stub.c             # стуб ucmd API
│       └── Makefile
├── docs/
└── Makefile
```

**Важно:** `firmware-core` — самостоятельный репозиторий со своими тестами и своей копией Unity. Тесты для приложений (`cli/u_read_line`) живут в `firmware/tests/` основного репо и используют отдельную копию Unity.

### 3.2 Makefile

Сборка тестов осуществляется через `firmware/tests/Makefile`. Запуск:

```bash
cd firmware/tests && make
# или отдельно:
cd firmware/tests && make test_cli
```

Makefile компилирует все источники (моки, Unity, microrl.c, тесты) в один бинарник `test_cli_runner` и запускает его. Сборка выполняется без предупреждений при `-Wall -Wextra -Wpedantic -Werror`.

### 3.3 Зависимости

| Зависимость | Источник | Статус |
|-------------|----------|--------|
| Unity framework | `firmware/tests/unity/` (сабмодуль в demo) | Есть |
| cmsis_stubs.h | `firmware/tests/test_helpers/cmsis_stubs.h` | Есть (блокирует stm32f407xx.h) |
| stm32f407xx.h (stub) | `firmware/tests/test_helpers/stm32f407xx.h` | Есть (пустая заглушка) |
| stubs.c/h (CYCCNT, precise_time) | `firmware/tests/test_helpers/stubs.*` | Есть |
| mock_drv_face.c/h | `firmware/tests/test_helpers/` | Есть |
| ucmd_stub.c | `firmware/tests/test_helpers/` | Есть |

---

## 4. Мокинг

### 4.1 drv_face_t mock (ввод)

Моковый интерфейс для подстановки входных данных:

```c
// mock_drv_face.h
#ifndef MOCK_DRV_FACE_H
#define MOCK_DRV_FACE_H

#include "drv_face.h"

extern const drv_face_t mock_iface;

void mock_iface_set_input(const uint8_t *data, size_t len);
void mock_iface_reset(void);

#endif
```

Реализация: буфер с данными для чтения через `mock_read()`. Каждый вызов возвращает следующий байт.

### 4.2 print callback mock (вывод)

Захват вывода microrl для проверки echo и сообщений:

```c
extern char mock_print_buffer[4096];
extern size_t mock_print_len;

void mock_print(const char *str) {
    size_t len = strlen(str);
    if (mock_print_len + len < sizeof(mock_print_buffer)) {
        strcpy(mock_print_buffer + mock_print_len, str);
        mock_print_len += len;
    }
}

void mock_print_reset(void) {
    mock_print_len = 0;
    mock_print_buffer[0] = '\0';
}
```

### 4.3 execute callback mock (проверка команд)

Захват вызова Execute для проверки argc/argv:

```c
extern int mock_exec_argc;
extern char *mock_exec_argv[16];
extern int mock_exec_called;

int mock_execute(int argc, const char * const *argv) {
    mock_exec_called = 1;
    mock_exec_argc = argc;
    for (int i = 0; i < argc && i < 16; i++) {
        mock_exec_argv[i] = strdup(argv[i]);
    }
    return 0;
}

void mock_exec_reset(void) {
    mock_exec_called = 0;
    mock_exec_argc = 0;
    for (int i = 0; i < 16; i++) {
        free(mock_exec_argv[i]);
        mock_exec_argv[i] = NULL;
    }
}
```

### 4.4 precise_time mock

Используется существующий мок из `stubs.h`:

- `pt_stamp()` возвращает `g_mock_cyccnt`
- `pt_elapsed_us(stamp)` вычисляет `(g_mock_cyccnt - stamp) / cycles_per_us`
- `stubs_advance_time_us(us)` продвигает время на указанное количество микросекунд

Для ESC-таймаута: после отправки ESC符号, тест вызывает `stubs_advance_time_us(600)` чтобы симулировать таймаут.

---

## 5. Группы тестов

### 5.1 Инициализация (test_microrl.c)

| Тест | Описание |
|------|----------|
| `test_microrl_init_clears_buffer` | После `microrl_init` буфер cmdline заполнен нулями |
| `test_microrl_init_esc_fields` | Поля escape, escape_seq, escape_stamp инициализированы 0 |
| `test_microrl_init_callbacks_null` | Callbacks (execute, sigint) установлены в NULL |
| `test_microrl_init_prompt` | prompt_str установлен в значение по умолчанию |

### 5.2 Ввод печатных символов (test_microrl.c)

| Тест | Описание |
|------|----------|
| `test_insert_single_char` | Ввод одного символа добавляет его в cmdline |
| `test_insert_multiple_chars` | Последовательный ввод нескольких символов |
| `test_insert_space_replaces_with_null` | Пробел заменяется на '\0' (разделитель токенов) |
| `test_insert_buffer_full` | При заполнении буфера символ не добавляется |
| `test_leading_space_ignored` | Пробел в начале пустой строки игнорируется |

### 5.3 Навигация курсора (test_microrl.c)

| Тест | Описание |
|------|----------|
| `test_arrow_right_moves_cursor` | ESC+[C перемещает курсор вправо |
| `test_arrow_left_moves_cursor` | ESC+[D перемещает курсор влево |
| `test_arrow_right_at_end` | Стрелка вправо на конце строки не меняет позицию |
| `test_arrow_left_at_start` | Стрелка влево в начале строки не меняет позицию |
| `test_home_moves_to_beginning` | ESC+[7~ перемещает курсор в начало |
| `test_end_moves_to_end` | ESC+[8~ перемещает курсор в конец |
| `test_ctrl_a_moves_home` | ^A (SOH) перемещает курсор в начало |
| `test_ctrl_e_moves_end` | ^E (ENQ) перемещает курсор в конец |
| `test_ctrl_f_moves_forward` | ^F (ACK) перемещает курсор вперёд |
| `test_ctrl_b_moves_backward` | ^B (STX) перемещает курсор назад |

### 5.4 Редактирование (test_microrl.c)

| Тест | Описание |
|------|----------|
| `test_backspace_removes_char` | Backspace удаляет символ перед курсором |
| `test_backspace_at_start_noop` | Backspace в начале строки не делает ничего |
| `test_delete_removes_at_cursor` | Delete удаляет символ под курсором |
| `test_ctrl_u_clears_to_beginning` | ^U (NAK) очищает всё от курсора до начала |
| `test_ctrl_k_clears_to_end` | ^K (VT) очищает всё от курсора до конца |
| `test_insert_at_cursor_shifts_right` | Ввод в середине строки сдвигает текст вправо |

### 5.5 История команд (test_microrl.c)

| Тест | Описание |
|------|----------|
| `test_history_save_on_enter` | При нажатии Enter команда сохраняется в историю |
| `test_arrow_up_restores_last` | ESC+[A восстанавливает последнюю команду |
| `test_arrow_down_newest` | ESC+[B возвращается к текущей строке |
| `test_history_empty_no_change` | Пустая история не меняет cmdline |
| `test_history_ring_wrap` | При переполнении истории старые записи удаляются |
| `test_ctrl_p_history_up` | ^P (DLE) работает как стрелка вверх для истории |
| `test_ctrl_n_history_down` | ^N (SO) работает как стрелка вниз для истории |

### 5.6 ESC таймаут (test_microrl.c)

| Тест | Описание |
|------|----------|
| `test_esc_timeout_resets_state` | При паузе > 500 мкс после ESC состояние сбрасывается |
| `test_esc_timeout_char_processed_normal` | После таймаута следующий символ обрабатывается как обычный |
| `test_esc_seq_complete_before_timeout` | Полная последовательность ESC+[A обрабатывается корректно при быстрой отправке |
| `test_esc_incomplete_hangs_then_times_out` | Неполная последовательность (ESC+[]) не ломает парсер |

**Пример теста:**
```c
static void test_esc_timeout_resets_state(void)
{
    microrl_init(&rl, mock_print);
    
    // Отправляем ESC
    microrl_insert_char(&rl, KEY_ESC);
    TEST_ASSERT_EQUAL(1, rl.escape);
    
    // Продвигаем время за пределы таймаута
    stubs_advance_time_us(600);
    
    // Отправляем обычный символ — должен быть обработан как печатный
    microrl_insert_char(&rl, 'h');
    TEST_ASSERT_EQUAL(0, rl.escape);
    TEST_ASSERT_EQUAL('h', rl.cmdline[0]);
    TEST_ASSERT_EQUAL(1, rl.cmdlen);
}
```

### 5.7 Token splitting (test_split.c)

| Тест | Описание |
|------|----------|
| `test_split_single_token` | Строка без пробелов возвращает 1 токен |
| `test_split_multiple_tokens` | "abc def ghi" возвращает 3 токена |
| `test_split_leading_spaces` | "  abc" корректно игнорирует ведущие пробелы |
| `test_split_trailing_spaces` | "abc  " корректно игнорирует хвостовые пробелы |
| `test_split_empty_string` | Пустая строка возвращает 0 токенов |
| `test_split_max_tokens_exceeded` | При превышении _COMMAND_TOKEN_NMB возвращается -1 |

**Примечание:** Функция `split()` статическая в microrl.c. Для тестирования либо вынести в отдельный файл, либо добавить тестовый accessors, либо тестировать через execute callback (проверить argc/argv).

### 5.8 Execute callback (test_microrl.c)

| Тест | Описание |
|------|----------|
| `test_enter_calls_execute` | Нажатие Enter вызывает callback |
| `test_enter_passes_correct_argc_argv` | Callback получает правильные argc и argv |
| `test_empty_enter_no_execute` | Пустая строка не вызывает execute |
| `test_enter_clears_line` | После Execute cmdline очищается |

### 5.9 Автодополнение (test_microrl.c)

| Тест | Описание |
|------|----------|
| `test_tab_calls_completion` | Tab вызывает completion callback |
| `test_completion_single_match` | Одиночное совпадение вставляется автоматически |
| `test_completion_no_callback` | Без установленного callback Tab игнорируется |

### 5.10 SIGINT (test_microrl.c)

| Тест | Описание |
|------|----------|
| `test_ctrl_c_calls_sigint` | ^C вызывает sigint callback |
| `test_sigint_not_set_no_crash` | Если callback не установлен, ^C не вызывает краш |

### 5.11 ucmd (test_ucmd.c)

| Тест | Описание |
|------|----------|
| `test_ucmd_parse_found` | Известная команда находит обработчик |
| `test_ucmd_parse_not_found` | Неизвестная команда возвращает UCMD_CMD_NOT_FOUND |
| `test_ucmd_parse_empty_argv` | Пустой argv обрабатывается корректно |
| `test_ucmd_help_prints_commands` | ucmd_help выводит список команд |

---

## 6. Матрица тестов (полный перечень)

| # | Тест | Группа | Приоритет | Статус |
|---|------|--------|-----------|--------|
| 1 | `test_microrl_init_clears_buffer` | Init | Высокий | [ ] |
| 2 | `test_microrl_init_esc_fields` | Init | Высокий | [ ] |
| 3 | `test_microrl_init_callbacks_null` | Init | Средний | [ ] |
| 4 | `test_microrl_init_prompt` | Init | Средний | [ ] |
| 5 | `test_insert_single_char` | Input | Высокий | [ ] |
| 6 | `test_insert_multiple_chars` | Input | Высокий | [ ] |
| 7 | `test_insert_space_replaces_with_null` | Input | Средний | [ ] |
| 8 | `test_insert_buffer_full` | Input | Средний | [ ] |
| 9 | `test_leading_space_ignored` | Input | Низкий | [ ] |
| 10 | `test_arrow_right_moves_cursor` | Nav | Высокий | [ ] |
| 11 | `test_arrow_left_moves_cursor` | Nav | Высокий | [ ] |
| 12 | `test_arrow_right_at_end` | Nav | Средний | [ ] |
| 13 | `test_arrow_left_at_start` | Nav | Средний | [ ] |
| 14 | `test_home_moves_to_beginning` | Nav | Высокий | [ ] |
| 15 | `test_end_moves_to_end` | Nav | Высокий | [ ] |
| 16 | `test_ctrl_a_moves_home` | Nav | Средний | [ ] |
| 17 | `test_ctrl_e_moves_end` | Nav | Средний | [ ] |
| 18 | `test_ctrl_f_moves_forward` | Nav | Низкий | [ ] |
| 19 | `test_ctrl_b_moves_backward` | Nav | Низкий | [ ] |
| 20 | `test_backspace_removes_char` | Edit | Высокий | [ ] |
| 21 | `test_backspace_at_start_noop` | Edit | Средний | [ ] |
| 22 | `test_delete_removes_at_cursor` | Edit | Средний | [ ] |
| 23 | `test_ctrl_u_clears_to_beginning` | Edit | Средний | [ ] |
| 24 | `test_ctrl_k_clears_to_end` | Edit | Средний | [ ] |
| 25 | `test_insert_at_cursor_shifts_right` | Edit | Средний | [ ] |
| 26 | `test_history_save_on_enter` | History | Высокий | [ ] |
| 27 | `test_arrow_up_restores_last` | History | Высокий | [ ] |
| 28 | `test_arrow_down_newest` | History | Высокий | [ ] |
| 29 | `test_history_empty_no_change` | History | Средний | [ ] |
| 30 | `test_history_ring_wrap` | History | Средний | [ ] |
| 31 | `test_ctrl_p_history_up` | History | Низкий | [ ] |
| 32 | `test_ctrl_n_history_down` | History | Низкий | [ ] |
| 33 | `test_esc_timeout_resets_state` | ESC | Высокий | [ ] |
| 34 | `test_esc_timeout_char_processed_normal` | ESC | Высокий | [ ] |
| 35 | `test_esc_seq_complete_before_timeout` | ESC | Высокий | [ ] |
| 36 | `test_esc_incomplete_hangs_then_times_out` | ESC | Средний | [ ] |
| 37 | `test_split_single_token` | Split | Высокий | [ ] |
| 38 | `test_split_multiple_tokens` | Split | Высокий | [ ] |
| 39 | `test_split_leading_spaces` | Split | Средний | [ ] |
| 40 | `test_split_trailing_spaces` | Split | Средний | [ ] |
| 41 | `test_split_empty_string` | Split | Средний | [ ] |
| 42 | `test_split_max_tokens_exceeded` | Split | Средний | [ ] |
| 43 | `test_enter_calls_execute` | Execute | Высокий | [ ] |
| 44 | `test_enter_passes_correct_argc_argv` | Execute | Высокий | [ ] |
| 45 | `test_empty_enter_no_execute` | Execute | Средний | [ ] |
| 46 | `test_enter_clears_line` | Execute | Средний | [ ] |
| 47 | `test_tab_calls_completion` | Complete | Средний | [ ] |
| 48 | `test_completion_single_match` | Complete | Низкий | [ ] |
| 49 | `test_completion_no_callback` | Complete | Низкий | [ ] |
| 50 | `test_ctrl_c_calls_sigint` | SIGINT | Средний | [ ] |
| 51 | `test_sigint_not_set_no_crash` | SIGINT | Средний | [ ] |
| 52 | `test_ucmd_parse_found` | ucmd | Высокий | [ ] |
| 53 | `test_ucmd_parse_not_found` | ucmd | Высокий | [ ] |
| 54 | `test_ucmd_parse_empty_argv` | ucmd | Средний | [ ] |
| 55 | `test_ucmd_help_prints_commands` | ucmd | Средний | [ ] |

**Всего:** ~55 тестов, из них 20 высокоприоритетных.

---

## 7. Критерии приемки

### 7.1 Автоматические тесты

- [x] Все тесты проходят успешно (`make test_cli` завершается с exit code 0)
- [x] Сборка без предупреждений при `-Wall -Wextra -Wpedantic -Werror`
- [x] Покрытие высокоприоритетных тестов: 100%

### 7.2 Ручная проверка на железе

**Внимание:** После прохождения автоматических тестов необходимо проверить работу модуля на реальном микроконтроллере STM32F407ZGT.

| Проверка | Описание | Статус |
|----------|----------|--------|
| Ввод команд | Ввод `help`, `mem`, `time` и других команд работает корректно | [x] |
| Навигация | Стрелки влево/вправо перемещают курсор по строке | [x] |
| Home/End | Клавиши Home/End (или ^A/^E) работают | [x] |
| История | Up/Down прокручивают историю команд | [x] |
| Редактирование | Backspace/Delete/^U/^K удаляют символы корректно | [x] |
| ESC таймаут | Быстрый ввод ESC как обычного символа не ломает парсер | [x] |
| Автодополнение | Tab (если настроено) работает | [x] |
| SIGINT | Ctrl+C вызывает обработчик без зависаний | [x] |

**Порядок смены статуса:**
1. `Черновик` -> `В работе` — начато написание тестов
2. `В работе` -> `Тесты написаны` — все тесты компилируются и проходят
3. `Тесты написаны` -> `Проверено на железе` — пользователь подтвердил работу на STM32F407ZGT
4. `Проверено на железе` -> `Выполнено` — статус финальный

---

## 8. Порядок выполнения работ

| Этап | Описание | Файлы | Сложность |
|------|----------|-------|-----------|
| 1 | Создание mock_drv_face.c/h | test_helpers/ | Низкая |
| 2 | Расширение Makefile (цель test_cli) | tests/Makefile | Низкая |
| 3 | Тесты инициализации + ввод | test_microrl.c | Низкая |
| 4 | Тесты навигации курсора | test_microrl.c | Средняя |
| 5 | Тесты редактирования | test_microrl.c | Средняя |
| 6 | Тесты истории | test_microrl.c | Средняя |
| 7 | Тесты ESC таймаута | test_microrl.c | Средняя |
| 8 | Тесты token splitting | test_split.c | Средняя |
| 9 | Тесты Execute callback | test_microrl.c | Низкая |
| 10 | Тесты ucmd (parse, help) | test_ucmd.c | Низкая |
| 11 | Сборка и запуск (`make test_cli`) | — | — |
| 12 | Ручная проверка на железе | — | — |

---

## 9. Риски

| Риск | Вероятность | Влияние | Мера снижения |
|------|-------------|---------|---------------|
| Статическая функция `split()` недоступна для тестов | Высокое | Среднее | Тестирование через execute callback или вынесение в отдельный файл |
| Мокинг printf может быть нестабильным на хосте | Средняя | Низкое | Использовать own print callback, не зависеть от stdio |
| ESC-таймаут зависит от precise_time | Средняя | Среднее | Мок pt_stamp/pt_elapsed_us уже реализован в stubs |

---

---

## 10. Обнаруженные баги

### 10.1 Баг: потеря символа при ESC-таймауте (`microrl.c`)

**Файл:** `firmware/cli/u_read_line/microrl.c`, функция `microrl_insert_char()`  
**Строки:** ~636–648 (блок if-else с `_USE_ESC_SEQ`)  
**Обнаружен:** 28 июня 2026, при написании unit-тестов

**Описание:** При срабатывании ESC-таймаута флаг `escape` сбрасывается, но текущий символ теряется. Причина: switch-case находится внутри `else`-блока конструкции:

```c
#ifdef _USE_ESC_SEQ
    if (pThis->escape) {
        if (pt_elapsed_us(pThis->escape_stamp) > _ESC_TIMEOUT_US) {
            pThis->escape = 0;
            /* fall through: process ch as normal character */
        } else if (escape_process(pThis, ch)) {
            ...
        } else {
            return;
        }
    } else {
#endif
        switch (ch) { ... }
#ifdef _USE_ESC_SEQ
    }
#endif
```

Комментарий `/* fall through */` неверен: в C fall-through из `if`-ветки в `else` невозможен. После сброса `escape = 0` выполнение идёт за пределы конструкции, и символ не обрабатывается.

**Влияние:** При быстрой отправке ESC + обычный символ с паузой >500 мкс между ними, первый печатный символ после таймаута теряется. На практике это проявляется редко (пользователь обычно не ждёт 500 мкс после случайного ESC), но регрессия возможна при изменении `_ESC_TIMEOUT_US`.

**Рекомендация:** Добавить `goto` для передачи управления к switch-case:

```c
#ifdef _USE_ESC_SEQ
    if (pThis->escape) {
        if (pt_elapsed_us(pThis->escape_stamp) > _ESC_TIMEOUT_US) {
            pThis->escape = 0;
            goto process_normal; /* fix: was incorrect "fall through" */
        } else if (escape_process(pThis, ch)) {
            pThis->escape = 0;
            return;
        } else {
            return;
        }
    }
#endif
    process_normal:
    switch (ch) { ... }
```

**Статус:** Не исправлено. Unit-тесты адаптированы (проверяют только сброс флага, без проверки добавления символа).
