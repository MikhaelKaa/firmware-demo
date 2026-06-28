<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright 2026 Michael Kaa -->

# Спецификация рефакторинга модуля u_read_line

**Дата:** 2026-06-25 (обновлено 2026-06-26)
**Автор:** Michael Kaa
**Статус:** Выполнен частично (коммит 2847322)

---

## 1. Обзор модуля

### 1.1 Назначение

Модуль `firmware/cli/u_read_line/` реализует интерфейс командной строки (CLI) для встроенной системы на базе STM32F407ZGT. Модуль обеспечивает:

- Чтение символов из потока ввода (UART/USB CDC)
- Редактирование строки команд с навигацией курсора (стрелки, Home, End)
- Историю команд (Up/Down)
- Автодополнение (Tab)
- Парсинг и диспетчеризацию пользовательских команд

### 1.2 Состав файлов

| Файл | Назначение | Статус авторства |
|------|-----------|-----------------|
| `ucmd.c` | Таблица команд, точка входа, обертка над microrl | Собственный код |
| `ucmd.h` | Публичный API модуля CLI | Собственный код |
| `microrl.c` | Ядро readline-библиотеки | Наbasis кода Eugene Samoylov (Helius) |
| `microrl.h` | Интерфейс microrl, структуры, константы | На basis кода Eugene Samoylov (Helius) |
| `config.h` | Конфигурация библиотеки microrl | На basis кода Eugene Samoylov (Helius) |
| `term_gxf.h` | ANSI-escape макросы для терминала | На basis идеи Zefick (Habr, 2017) |
| `README` | Документация оригинальной библиотеки | Оригинальная документация Helius |

### 1.3 Текущее состояние

Модуль в целом работает стабильно. Основные проблемы (исправлены):

- [x] Дублирующие include и предупреждения компилятора
- [x] Нарушение архитектурной изоляции слоев (CMSIS в CLI) — контролируемое исключение
- [x] Использование `scanf` (stdio) вместо прямого чтения через `drv_face_t`
- [x] Race condition при обработке ESC-последовательностей — ESC-таймаут
- [x] Нестыковка стилистики с остальными модулями проекта

---

## 2. Атрибуция авторов

### 2.1 Оригинальная библиотека microrl

Библиотека `microrl` была создана Eugene Samoylov (Helius, ghelius@gmail.com).
Оригинальный репозиторий: <https://github.com/helius/microrl>
Оригинальная лицензия не указана явно; код используется с явного разрешения правообладателя.

### 2.2 term_gxf.h

Идея ANSI-escape макросов принадлежит Zefick, статья «Терминальная графика» на Habr (28 марта 2017).
Ссылка: <https://habr.com/ru/articles/325082/>

### 2.3 Требования по атрибуции

Во все файлы, происходящие из сторонних проектов, добавить заголовок:

```c
/*
 * Based on microrl library by Eugene Samoylov (Helius) <ghelius@gmail.com>
 * Original: https://github.com/helius/microrl
 * Modified for firmware-demo project by Michael Kaa
 */
```

Для `term_gxf.h` сохранить существующий комментарий о Zefick.

---

## 3. Выявленные проблемы

### 3.1 Ошибки в ucmd.c

| # | Строка | Проблема | Приоритет |
|---|--------|----------|-----------|
| # | Описание | Статус |
|---|----------|--------|
| E1 | Дублирующий `#include "microrl.h"` | [x] Исправлено |
| E2 | `#include "stm32f407xx.h"` нарушает изоляцию слоев | [x] Контролируемое исключение с комментарием |
| E3 | `volatile uint8_t ucmd_default_rx` — volatile не нужен | [x] Удалено (заменён подход чтения) |
| E4 | `uint8_t` для `scanf("%c")` — требуется `int` по стандарту C99 | [x] Заменён scanf на drv_face_t->read() |
| E5 | Использование `scanf` (stdio) вместо прямого чтения из драйвера | [x] Заменён на drv_face_t->read() |
| E6 | `(const char **)argv` — нарушение const-correctness | [x] Исправлено |
| E7 | Разрозненный вывод "unknown command" | [x] Упрощено, убран лишний каст |
| E8 | Лишний каст `(char*)&argv[i][0]` | [x] Заменён на `argv[i]` |
| E9 | `ucmd_mcu_reset()` с прямым вызовом NVIC_SystemReset() | [x] Оставлено как контролируемое исключение |

### 3.2 Предупреждения компилятора в microrl.c

| # | Строка | Проблема | Приоритет |
|---|--------|----------|-----------|
| # | Описание | Статус |
|---|----------|--------|
| W1 | `<stdlib.h>` подключен, но не используется | [x] Удалён |
| W2 | Поля `escape_seq`, `escape` не инициализируются в `microrl_init()` | [x] Инициализированы (+ escape_stamp) |
| W3 | Switch-case fall-through без явных комментариев | [x] Добавлены break / комментарии |

### 3.3 Архитектурные проблемы

| # | Описание | Приоритет |
|---|----------|-----------|
| # | Описание | Статус |
|---|----------|--------|
| A1 | ESC-последовательности "разрезаются" при быстром вызове `ucmd_default_proc()` | [x] Добавлен ESC-таймаут (_ESC_TIMEOUT_US) |
| A2 | Чтение ввода через stdio (`scanf`) вместо прямого доступа к `drv_face_t` | [x] Заменён на drv_face_t->read() |
| A3 | Отсутствие rate limiting — функция обрабатывается слишком часто | [ ] Не применён (ломает ESC при 115200 бод) |

### 3.4 Стилистические проблемы

| # | Описание | Приоритет |
|---|----------|-----------|
| # | Описание | Статус |
|---|----------|--------|
| S1 | `print_help_cb` не соответствует неймингу других CLI-модулей (`ucmd_*`) | [x] Переименована в `ucmd_help` |
| S2 | Отсутствие Doxygen-комментариев к публичным функциям в ucmd.h | [x] Добавлены |
| S3 | Смешанные стили комментариев (русские, английские, отсутствие) | [x] Унифицированы (Doxygen) |

---

## 4. Цели рефакторинга

После рефакторинга модуль должен:

1. **Собираться без предупреждений** при `-Wall -Wextra -pedantic`
2. **Не нарушать изоляцию слоев** — отсутствие CMSIS в коде, не относящемся к драйверам
3. **Корректно обрабатывать ESC-последовательности** независимо от частоты вызова `proc()`
4. **Использовать чтение через `drv_face_t`** вместо `scanf`
5. **Соответствовать стилистике проекта** — snake_case, Doxygen-комментарии, структура кода
6. **Содержать атрибуцию авторов** во всех файлах на basis стороннего кода

---

## 5. Подробные изменения по файлам

### 5.1 `config.h`

#### 5.1.1 Атрибуция

Добавить заголовок (см. раздел 2.3).

#### 5.1.2 Добавление ESC-таймаута [ВЫПОЛНЕНО]

```c
#define _ESC_TIMEOUT_US  500
```

Таймаут для незавершённых ESC-последовательностей (в микросекундах). Если после получения символа ESC продолжение не приходит в течение этого интервала, состояние escape сбрасывается и ожидаемый символ обрабатывается как обычный печатаемый символ.

#### 5.1.3 Очистка [ВЫПОЛНЕНО]

Убрать закомментированные варианты конфигурации, оставив только активные настройки. Сделано.

---

### 5.2 `microrl.h`

#### 5.2.1 Атрибуция

Добавить аналогичный заголовок атрибуции.

#### 5.2.2 Изменение структуры microrl_t [ВЫПОЛНЕНО]

Добавлено поле для ESC-таймаута рядом с существующими полями `escape_seq` и `escape`:

```c
#ifdef _USE_ESC_SEQ
    uint32_t escape_stamp;   /* timestamp of last ESC character (pt_stamp) */
#endif
```

#### 5.2.3 Исправление определений true/false [ВЫПОЛНЕНО]

Заменены ручные `#define true 1 / #define false 0` на подключение `<stdbool.h>`. Сделано.

---

### 5.3 `microrl.c`

#### 5.3.1 Атрибуция

Добавить заголовок атрибуции.

#### 5.3.2 Удаление неиспользуемых include

Убрать `#include <stdlib.h>` (строка 10).

#### 5.3.3 Инициализация полей в microrl_init()

```c
#ifdef _USE_ESC_SEQ
    pThis->escape = 0;
    pThis->escape_seq = 0;
    pThis->escape_stamp = 0;
#endif
```

#### 5.3.4 Подключение precise_time.h

Добавить `#include "precise_time.h"` для реализации ESC-таймаута.

**Обоснование:** Файл microrl.c находится в слое `cli/u_read_line`, а не в чистой бизнес-логике (`app`). Использование CMSIS через precise_time.h допустимо, так как это часть infrastructure.

#### 5.3.5 Реализация ESC-таймаута

В `microrl_insert_char()` при входящем ESC установить таймстемп:

```c
case KEY_ESC:
#ifdef _USE_ESC_SEQ
                pThis->escape = 1;
                pThis->escape_stamp = pt_stamp();
#endif
            break;
```

При проверке escape-состояния добавить проверку таймаута:

```c
#ifdef _USE_ESC_SEQ
    if (pThis->escape) {
        if (pt_elapsed_us(pThis->escape_stamp) > _ESC_TIMEOUT_US) {
            pThis->escape = 0;
            /* Fall through to process ch as normal character */
        } else if (escape_process(pThis, ch)) {
            pThis->escape = 0;
        }
    } else {
#endif
```

#### 5.3.6 Стилизация функций

- Добавить Doxygen-комментарии к публичным функциям
- Унифицировать отступы по образцу `ucmd_adc.c`
- Добавить комментарии `// fall through` где это намеренное поведение

---

### 5.4 `ucmd.h`

#### 5.4.1 Doxygen-комментарии

Добавить документацию ко всем публичным функциям и типам (`command_t`, `command_cb`, `ucmd_default_init`, `ucmd_default_proc`, `ucmd_parse`, `ucmd_help`, `ucmd_default_print`, `ucmd_set_sigint`).

---

### 5.5 `ucmd.c`

#### 5.5.1 Удаление дублирующего include

Убрать строку 8 (`#include "microrl.h"` — дубль строки 7).

#### 5.5.2 Изоляция ucmd_mcu_reset()

Функция `ucmd_mcu_reset()` напрямую использует CMSIS (`NVIC_SystemReset`). Полный перенос команды `reset` в отдельный подмодуль CLI запланирован, но выходит за рамки текущего рефакторинга.

**Решение (текущий цикл):** Оставить функцию рядом с основными файлами модуля с явным комментарием-исключением:

```c
/*
 * NOTE: Controlled layer violation — this function directly uses CMSIS
 * (NVIC_SystemReset). Planned for migration to a dedicated module
 * in a future refactoring cycle.
 */
#include "stm32f407xx.h"
```

#### 5.5.3 Исправление типа ucmd_default_rx

Заменить `static volatile uint8_t ucmd_default_rx;` на что-то подходящее для нового подхода с `drv_face_t` (см. 5.5.4).

#### 5.5.4 Замена scanf на чтение через drv_face_t [ВЫПОЛНЕНО]

**Было:**
```c
void ucmd_default_proc(void) {
    scanf("%c", &ucmd_default_rx);
    microrl_insert_char(&default_rl, ucmd_default_rx);
}
```

**Проблема:** `scanf` — часть stdio, которая абстрагируется над драйверами. Прямое чтение из `drv_face_t` обеспечивает независимость от stdio, легкий мокинг в unit-тестах и возможность выбора любого источника ввода (UART, USB CDC).

**Стало:**
```c
#include "precise_time.h"
#include "drv_face.h"

static const drv_face_t *ucmd_iface;

void ucmd_default_init(void) {
    setvbuf(stdin, NULL, _IONBF, 0);   // оставлено — см. примечание ниже
    setvbuf(stdout, NULL, _IONBF, 0);  // оставлено — см. примечание ниже
    ucmd_iface = dev_uart1_get();
    microrl_init(&default_rl, ucmd_default_print);
    microrl_set_execute_callback(&default_rl,
        (int (*)(int, const char * const*))ucmd_execute);
    microrl_set_sigint_callback(&default_rl, default_sigint);
    microrl_insert_char(&default_rl, '\n');
    microrl_insert_char(&default_rl, '\n');
}

void ucmd_default_proc(void) {
    uint8_t ch;
    int ret = ucmd_iface->read(&ch, 1);
    if (ret <= 0) return;
    microrl_insert_char(&default_rl, ch);
}
```

**Примечание:** `setvbuf(stdin/stdout, NULL, _IONBF, 0)` оставлено осознанно и НЕ должно быть удалено:
- `stdout` используется для вывода (printf в ucmd_default_print и всех CLI-командах) — без `_IONBF` вывод будет буферизироваться с задержками отображения echo и сообщений.
- `stdin` не используется microrl напрямую, но отключение его буферизации гарантирует предсказуемое поведение терминала при использовании stdio другими модулями системы (syscalls, перенаправление потока и т.д.).

#### 5.5.5 Исправление const-correctness в ucmd_execute

Изменить сигнатуру на `int ucmd_execute(int argc, const char * const *argv);` для согласования с callback в microrl.h.

#### 5.5.6 Переименование print_help_cb -> ucmd_help

#### 5.5.7 Исправление вывода "unknown command"

Упростить `(char*)&argv[i][0]` до `argv[i]`, добавить двоеточие после "unknown command".

#### 5.5.8 Обновление таблицы команд

После переименования `print_help_cb` на `ucmd_help`.

#### 5.5.9 Добавление Doxygen-комментариев

---

### 5.6 `term_gxf.h`

Без изменений. Атрибуция уже присутствует.

### 5.7 `README`

Добавить секцию о модификациях для firmware-demo (ESC timeout, rate limiting, fixed init).

---

## 6. Решение проблемы ESC-последовательностей

### 6.1 Описание проблемы

При быстрой последовательности вызовов `ucmd_default_proc()`, когда скорость вызовов превышает скорость поступления символов по UART, ESC-последовательности могут быть "разрезаны":

1. Приходит `\033` -> устанавливается `escape = 1`
2. Следующий вызов обрабатывает мусор/старое значение как продолжение ESC
3. Парсер интерпретирует неверный символ

### 6.2 Двухуровневая защита

**Level 1:** Rate limiting в `ucmd_default_proc()` через `precise_time.h` (интервал 500 мкс, ~2 kHz).

**Level 2:** ESC-таймаут в `microrl_insert_char()` — при поступлении ESC установить `escape_stamp = pt_stamp()`, при следующем символе проверить таймаут.

### 6.3 Зависимости

- `firmware/core/lib/time/precise_time.h` — функции `pt_stamp()`, `pt_elapsed_us()`
- Требуется предварительный вызов `pt_init()` в startup-коде

---

## 7. Таблица изменений API

| Функция / тип | Было | Стало | Совместимость |
|--------------|------|-------|---------------|
| `print_help_cb(int, char**)` | свободная функция | `ucmd_help(int, char**)` | Breaking |
| `ucmd_execute(int argc, char **argv)` | argv без const | `const char * const *argv` | Breaking |
| структура `microrl_t` | нет escape_stamp | добавлено `uint32_t escape_stamp` | Binary compatible |
| `ucmd_default_proc()` | scanf + без rate limiting | `drv_face_t->read()` + rate limiting | Compatible |
| `_ESC_TIMEOUT_US` | не существует | новое макроопределение | N/A |

---

## 8. Порядок выполнения работ

| Этап | Описание | Файлы | Сложность |
|------|----------|-------|-----------|
| 1 | Атрибуция авторов | microrl.c, microrl.h, config.h, README | Низкая |
| 2 | Исправление ucmd.c | ucmd.c, ucmd.h | Средняя |
| 3 | Рефакторинг microrl | microrl.c, microrl.h | Низкая |
| 4 | ESC-таймаут + precise_time.h | microrl.c, microrl.h, config.h | Средняя |
| 5 | drv_face_t + rate limiting | ucmd.c | Средняя |
| 6 | Стилистика (Doxygen, нейминг) | все файлы модуля | Низкая |
| 7 | Сборка и проверка | — | — |

---

## 9. Критерии приемки

1. **Сборка** — без предупреждений при `-Wall -Wextra -pedantic`
2. **Атрибуция** — все файлы на basis стороннего кода содержат заголовок автора
3. **Изоляция слоев** — CMSIS не подключается из app-слоя (за исключением контролируемых исключений)
4. **ESC-таймаут** — корректный сброс состояния парсера при паузе > 500 мкс
5. **Rate limiting** — пропуск вызовов чаще 2 kHz без потери данных
6. **Стилистика** — Doxygen, snake_case
7. **Функциональность** — все команды CLI работают (`help`, `reset`, `mem`, `time`, `led`, `w25q`, `usb`, `adc`)

---

## 10. Риски и меры снижения

| Риск | Вероятность | Влияние | Мера снижения |
|------|-----------|---------|--------------|
| Изменение сигнатуры callback нарушит другие модули CLI | Средняя | Высокое | Тщательная проверка всех extern объявлений |
| Rate limiting потеряет символы при пиковой нагрузке | Низкая | Среднее | Символы буферизуются драйвером UART — потерь не будет |
| ESC-таймаут 500 мкс слишком мал/велик | Средняя | Среднее | Значение настраивается через config.h |

---

## 11. Unit-тесты

### 11.1 Цели

После рефакторинга модуль должен быть тестируем на хосте (gcc x86_64) без зависимости от CMSIS. Тесты покрывают:

- Парсинг ESC-последовательностей (стрелки, Home, End)
- ESC-таймаут (сброс состояния при паузе > `_ESC_TIMEOUT_US`)
- Историю команд (Up/Down, кольцевой буфер, переполнение)
- Разбиение строки на токены (split)
- Обработку спецсимволов (^A, ^E, ^U, ^K, Backspace, Delete)
- Автодополнение (Tab)
- Rate limiting в `ucmd_default_proc()`

### 11.2 Инфраструктура

Тесты располагаются в `firmware/core/tests/cli/u_read_line/` с использованием:

- Unity framework (`firmware/core/tests/unity/`)
- Существующих стубов (`test_helpers/stubs.h`, `cmsis_stubs.h`)
- Мокинг `drv_face_t` для подмены источника ввода и перехвата вывода

### 11.3 Мокинг через drv_face_t

Переход на `drv_face_t` позволяет полностью изолировать microrl от железа:

```c
static uint8_t mock_rx_buf[256];
static size_t mock_rx_pos;
static char mock_tx_buf[1024];
static size_t mock_tx_len;

static int mock_read(void *buf, size_t len) {
    if (mock_rx_pos >= sizeof(mock_rx_buf)) return 0;
    memcpy(buf, &mock_rx_buf[mock_rx_pos++], 1);
    return 1;
}

static const drv_face_t mock_iface = {
    .read  = mock_read,
    .write = NULL,
    .ioctl = NULL
};
```

### 11.4 Ключевые тест-кейсы

| Тест | Описание | Приоритет |
|------|----------|-----------|
| `test_esc_arrow_up` | `ESC+[A` вызывает навигацию по истории (Up) | Высокий |
| `test_esc_arrow_down` | `ESC+[B` вызывает навигацию по истории (Down) | Высокий |
| `test_esc_home_end` | `ESC+[7~` / `ESC+[8~` перемещают курсор | Высокий |
| `test_esc_timeout_reset` | При паузе >500 мкс после ESC символ обрабатывается как обычный | Высокий |
| `test_split_tokens` | `split()` корректно разделяет строку на токены | Высокий |
| `test_history_ring_wrap` | Кольцевой буфер переполняется без потери данных | Средний |
| `test_backspace_at_start` | Backspace в начале строки не ломает курсор | Средний |
| `test_ctrl_u_clear` | ^U очищает всю строку | Средний |
| `test_exec_callback` | При Enter вызывается callback с правильными аргументами | Высокий |

### 11.5 Интеграция в Makefile

- Расширить паттерн в `firmware/core/tests/Makefile` на `cli/u_read_line/test_*.c`
- Добавить `-I../../cli/u_read_line` в INCLUDES
- Ссылать стубы printf/fgetc из test_helpers

---

## 12. Пасхалка: «GO SLEEP, STUPID USER!»

В функции `ucmd_execute()` присутствует сообщение-пасхалка, выводимое при повторном вводе неизвестных команд (после 6 попыток):

```c
set_display_atrib(F_RED);
printf("GO SLEEP, STUPID USER!\r\n");
resetcolor();
```

**Происхождение:** отсылка к прошивке *Evo Reset Service* — сервисному меню ZX-совместимого компьютера **ZX-Evolution (Pentevo)** от группы разработчиков NedoPC.

**Источник:** <http://forum.nedopc.com/viewtopic.php?f=33&t=861&start=40>
**Источник:** <https://hermitlair.ucoz.com/blog/2017-07-14-937>

**Решение:** оставить без изменений — сообщение не влияет на функциональность и является культурным артефактом проекта.