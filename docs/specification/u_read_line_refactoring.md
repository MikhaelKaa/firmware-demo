<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright 2026 Michael Kaa -->

# Спецификация рефакторинга модуля u_read_line

**Дата:** 2026-06-25  
**Автор:** Michael Kaa  
**Статус:** Черновик

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

Модуль в целом работает стабильно. Основные проблемы:

- Дублирующие include и предупреждения компилятора
- Нарушение архитектурной изоляции слоев (CMSIS в CLI)
- Блокирующий `scanf` вместо неблокирующего чтения
- Race condition при обработке ESC-последовательностей при быстрой отсылке символов
- Нестыковка стилистики с остальными модулями проекта

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
| E1 | 7-8 | Дублирующий `#include "microrl.h"` | Высокий |
| E2 | 35 | `#include "stm32f407xx.h"` нарушает изоляцию слоев (CMSIS в app-слое) | Высокий |
| E3 | 134 | `volatile uint8_t ucmd_default_rx` — volatile не нужен, переменная не в прерывании | Средний |
| E4 | 134 | `uint8_t` для `scanf("%c")` — требуется `int` по стандарту C99 | Высокий |
| E5 | 156 | Блокирующий `scanf` в main-loop функции | Критический |
| E6 | 95-96 | `(const char **)argv` — нарушение const-correctness (discards qualifier) | Средний |
| E7 | 99-103 | Разрозненный вывод сообщения "unknown command" без завершающего `\r\n` в одной строке | Низкий |
| E8 | 101 | Лишний каст `(char*)&argv[i][0]`, достаточно `argv[i]` | Низкий |
| E9 | 36-41 | `ucmd_mcu_reset()` содержит прямой вызов `NVIC_SystemReset()` (CMSIS) | Высокий |

### 3.2 Предупреждения компилятора в microrl.c

| # | Строка | Проблема | Приоритет |
|---|--------|----------|-----------|
| W1 | 10 | `<stdlib.h>` подключен, но не используется | Средний |
| W2 | 319-340 | Поля `escape_seq`, `escape` не инициализируются в `microrl_init()` | Высокий |
| W3 | 572-700 | Switch-case fall-through без явных комментариев (может генерировать варнинги при `-Wimplicit-fallthrough`) | Средний |

### 3.3 Архитектурные проблемы

| # | Описание | Приоритет |
|---|----------|-----------|
| A1 | ESC-последовательности "разрезаются" при быстром вызове `ucmd_default_proc()` — состояние escape-парсера рассинхронизируется | Критический |
| A2 | Блокирующий `scanf` в функции, предназначенной для вызова в main loop | Высокий |
| A3 | Отсутствие rate limiting — функция обрабатывается слишком часто | Средний |

### 3.4 Стилистические проблемы

| # | Описание | Приоритет |
|---|----------|-----------|
| S1 | `print_help_cb` не соответствует неймингу других CLI-модулей (`ucmd_*`) | Низкий |
| S2 | Отсутствие Doxygen-комментариев к публичным функциям в ucmd.h | Средний |
| S3 | Смешанные стили комментариев (русские, английские, отсутствие) | Низкий |

---

## 4. Цели рефакторинга

После рефакторинга модуль должен:

1. **Собираться без предупреждений** при `-Wall -Wextra -pedantic`
2. **Не нарушать изоляцию слоев** — отсутствие CMSIS в коде, не относящемся к драйверам
3. **Корректно обрабатывать ESC-последовательности** независимо от частоты вызова `proc()`
4. **Использовать неблокирующее чтение** вместо `scanf`
5. **Соответствовать стилистике проекта** — snake_case, Doxygen-комментарии, структура кода
6. **Содержать атрибуцию авторов** во всех файлах на basis стороннего кода

---

## 5. Подробные изменения по файлам

### 5.1 `config.h`

#### 5.1.1 Атрибуция

Добавить заголовок:
```c
/*
 * Based on microrl library by Eugene Samoylov (Helius) <ghelius@gmail.com>
 * Original: https://github.com/helius/microrl
 * Modified for firmware-demo project by Michael Kaa
 */
```

#### 5.1.2 Добавление ESC-таймаута

Добавить макроопределение для таймаута ESC-последовательностей:

```c
/*
 * Timeout for incomplete ESC sequences (in microseconds).
 * If after receiving ESC character, the continuation does not arrive
 * within this interval, the escape state is reset and the pending
 * character is processed as a normal printable character.
 */
#define _ESC_TIMEOUT_US  500
```

#### 5.1.3 Очистка

Убрать закомментированные варианты конфигурации, оставив только активные настройки с комментариями о доступных альтернативах в одном месте.

---

### 5.2 `microrl.h`

#### 5.2.1 Атрибуция

Добавить аналогичный заголовок атрибуции.

#### 5.2.2 Изменение структуры microrl_t

Добавить поле для ESC-таймаута:

```c
#ifdef _USE_ESC_SEQ
    uint32_t escape_stamp;   /* timestamp of last ESC character (pt_stamp) */
#endif
```

Положение в структуре — рядом с существующими полями `escape_seq` и `escape`.

#### 5.2.3 Исправление определений true/false

Заменить ручные `#define true 1 / #define false 0` на подключение `<stdbool.h>`:

```c
#include <stdbool.h>
```

И убрать ручные дефайны (строки 6-7).

---

### 5.3 `microrl.c`

#### 5.3.1 Атрибуция

Добавить заголовок атрибуции.

#### 5.3.2 Удаление неиспользуемых include

Убрать `#include <stdlib.h>` (строка 10) — не используется.

#### 5.3.3 Инициализация полей в microrl_init()

В `microrl_init()` добавить инициализацию escape-полей:

```c
#ifdef _USE_ESC_SEQ
    pThis->escape = 0;
    pThis->escape_seq = 0;
    pThis->escape_stamp = 0;
#endif
```

#### 5.3.4 Подключение precise_time.h

Добавить `#include "precise_time.h"` для реализации ESC-таймаута.

**Обоснование:** Файл microrl.c находится в слое `cli/u_read_line`, а не в чистой бизнес-логике (`app`). Использование CMSIS через precise_time.h допустимо, так как это часть infrastructure, а не логики приложения.

#### 5.3.5 Реализация ESC-таймаута

В функции `microrl_insert_char()` заменить текущую логику escape-обработки:

**Было (строки 566-570):**
```c
#ifdef _USE_ESC_SEQ
    if (pThis->escape) {
        if (escape_process(pThis, ch))
            pThis->escape = 0;
    } else {
#endif
```

**Стало:**
```c
#ifdef _USE_ESC_SEQ
    if (pThis->escape) {
        /* Check timeout — if ESC continuation did not arrive in time, reset state */
        if (pt_elapsed_us(pThis->escape_stamp) > _ESC_TIMEOUT_US) {
            pThis->escape = 0;
            /* Fall through to process ch as a normal character */
        } else if (escape_process(pThis, ch)) {
            pThis->escape = 0;
        }
    } else {
#endif
```

В case KEY_ESC добавить установку метки времени:

```c
case KEY_ESC:
#ifdef _USE_ESC_SEQ
                pThis->escape = 1;
                pThis->escape_stamp = pt_stamp();
#endif
            break;
```

#### 5.3.6 Стилизация функций

Привести функции к единому стилю:
- Добавить Doxygen-комментарии к публичным функциям
- Унифицировать отступы и расстановку фигурных скобок по образцу `ucmd_adc.c`
- Добавить комментарии `// fall through` где это намеренное поведение

---

### 5.4 `ucmd.h`

#### 5.4.1 Doxygen-комментарии

Добавить документацию ко всем публичным функциям и типам:

```c
/**
 * @brief Код возврата, указывающий что команда не найдена в таблице.
 */
#define UCMD_CMD_NOT_FOUND INT_MIN

/**
 * @brief Тип функции-обработчика команды.
 *
 * @param argc  Количество аргументов.
 * @param argv  Массив строк-аргументов.
 * @return      0 при успехе, отрицательный код ошибки при сбое.
 */
typedef int (*command_cb)(int argc, char **argv);

/**
 * @brief Структура описания одной команды CLI.
 */
typedef struct command {
    const char *cmd;       /**< Строка команды для сравнения */
    const char *help;      /**< Справка по команде */
    command_cb fn;         /**< Функция-обработчик */
} command_t;

/**
 * @brief Инициализация CLI с настройками по умолчанию.
 *
 * Вызывать один раз при старте системы после инициализации подсистемы ввода/вывода.
 */
void ucmd_default_init(void);

/**
 * @brief Периодическая обработка входящих символов (неблокирующая).
 *
 * Вызывать из main loop. Функция читает доступный символ (если есть)
 * и передает его в readline-парсер. Включен rate limiting.
 */
void ucmd_default_proc(void);

/**
 * @brief Парсинг команды из таблицы и её выполнение.
 *
 * @param cmd_list  Таблица команд для поиска.
 * @param argc      Количество аргументов.
 * @param argv      Массив строк-аргументов.
 * @return          Код возврата обработчика или UCMD_CMD_NOT_FOUND.
 */
int ucmd_parse(command_t cmd_list[], int argc, const char **argv);

/**
 * @brief Обработчик команды "help".
 *
 * Выводит список доступных команд и их описание.
 * @param argc  Количество аргументов (игнорируется).
 * @param argv  Массив аргументов (игнорируется).
 * @return      0 при успехе.
 */
int ucmd_help(int argc, char *argv[]);

/**
 * @brief Callback для вывода строк в readline.
 *
 * @param str  Строка для вывода.
 */
void ucmd_default_print(const char *str);

/**
 * @brief Установка функции обработки SIGINT (Ctrl+C).
 *
 * @param sigintf  Функция обратного вызова при нажатии Ctrl+C.
 */
void ucmd_set_sigint(void (*sigintf)(void));
```

---

### 5.5 `ucmd.c`

#### 5.5.1 Удаление дублирующего include

Убрать строку 8 (`#include "microrl.h"` — дубль строки 7).

#### 5.5.2 Вынос ucmd_mcu_reset() из модуля

**Вариант А (предпочтительный):** Перенести функцию `ucmd_mcu_reset()` и связанную с ней зависимость от `stm32f407xx.h` в отдельный файл `firmware/cli/system/ucmd_system.c`:

```c
/* ucmd_system.c — CLI-команды системного управления */
#include "stm32f407xx.h"
#include "ucmd.h"

int ucmd_mcu_reset(int argc, char **argv) {
    (void)argc;
    (void)argv;
    NVIC_SystemReset();
    return 0;  /* never reached */
}
```

В `ucmd.c` оставить только `extern int ucmd_mcu_reset(int argc, char **argv);` и запись в таблицу команд.

**Вариант Б:** Если создание нового модуля нецелесообразно — оставить функцию в ucmd.c, но явно пометить нарушением слоя с комментарием:

```c
/*
 * NOTE: This function directly uses CMSIS (NVIC_SystemReset).
 * This is a controlled exception — system reset has no driver abstraction.
 * Located here temporarily until a dedicated system CLI module is created.
 */
#include "stm32f407xx.h"
```

#### 5.5.3 Исправление типа ucmd_default_rx

Заменить:
```c
static volatile uint8_t ucmd_default_rx;
```
На:
```c
static int ucmd_default_rx;
```

`volatile` не требуется (переменная используется только в main контексте, не в прерываниях). Тип `int` — требование стандарта C99 для `%c` в scanf/fscanf.

#### 5.5.4 Замена scanf на неблокирующее чтение

**Текущий код:**
```c
void ucmd_default_proc(void) {
    scanf("%c", &ucmd_default_rx);
    microrl_insert_char(&default_rl, ucmd_default_rx);
}
```

**Новый код (с rate limiting):**
```c
#include "precise_time.h"

/* Rate limit: skip calls faster than 500 us (2 kHz max) */
#define UCMD_PROC_INTERVAL_US  500

static uint32_t ucmd_last_stamp;
static bool ucmd_stamp_initialized;

void ucmd_default_proc(void) {
    /* Rate limiting */
    if (!ucmd_stamp_initialized) {
        ucmd_last_stamp = pt_stamp();
        ucmd_stamp_initialized = true;
        return;
    }

    uint32_t dt = pt_elapsed_us(ucmd_last_stamp);
    if (dt < UCMD_PROC_INTERVAL_US) {
        return;
    }
    ucmd_last_stamp = pt_stamp();

    /* Non-blocking read from stdin */
    int ch = fgetc(stdin);
    if (ch == EOF) {
        return;
    }
    ucmd_default_rx = (uint8_t)ch;
    microrl_insert_char(&default_rl, ucmd_default_rx);
}
```

**Примечание:** Если `fgetc(stdin)` остается блокирующим на целевой платформе, рассмотреть альтернативы:
- Прямой вызов `uart1_read_nonblocking()` из драйвера UART
- Чтение из кольцевого буфера, заполняемого прерыванием UART

#### 5.5.5 Исправление const-correctness в ucmd_execute

**Текущий код:**
```c
int ucmd_execute(int argc, char **argv) {
    int ret = 0;
    ret = ucmd_parse(cmd_list, argc, (const char **)argv);
```

**Проблема:** `(const char **)argv` — приведение убирает квалификатор `const`, что является нарушением const-correctness и может вызывать неопределенное поведение.

**Решение 1 (предпочтительное):** Изменить сигнатуру `ucmd_execute` на:
```c
int ucmd_execute(int argc, const char * const *argv);
```

Это согласуется с подписью callback в microrl.h:
```c
int (*execute)(int argc, const char * const *argv);
```

Тогда приведение не потребуется.

**Решение 2:** Если изменение сигнатуры нарушает существующие обработчики — оставить как есть, но явно пометить комментарий о безопасности (так как argv никогда не модифицируется через указатель).

#### 5.5.6 Переименование print_help_cb -> ucmd_help

Для соответствия неймингу остальных CLI-модулей:
```c
/* Было */
int print_help_cb(int argc, char *argv[])

/* Стало */
int ucmd_help(int argc, char *argv[])
```

Обновить все места использования (таблица команд, заголовочный файл).

#### 5.5.7 Исправление вывода "unknown command"

**Текущий код:**
```c
if(ret == UCMD_CMD_NOT_FOUND){
    static uint8_t gssu = 0;
    if(gssu++ < 6)  {
      printf("unknown command");
      for(int i = 0; i < argc; i++) {
        printf(" %s", (char*)&argv[i][0]);
      }
      printf(", try help\r\n");
    } else {
      gssu = 0;
      set_display_atrib(F_RED);
      printf("GO SLEEP, STUPID USER!\r\n");
      resetcolor();
    }
}
```

**Новый код:**
```c
if (ret == UCMD_CMD_NOT_FOUND) {
    static uint8_t gssu = 0;
    if (gssu++ < 6) {
        printf("unknown command:");
        for (int i = 0; i < argc; i++) {
            printf(" %s", argv[i]);
        }
        printf(", try help\r\n");
    } else {
        gssu = 0;
        set_display_atrib(F_RED);
        printf("GO SLEEP, STUPID USER!\r\n");
        resetcolor();
    }
}
```

Изменения:
- `argv[i]` вместо `(char*)&argv[i][0]`
- `"unknown command:"` с двоеточием вместо `, try help`
- Форматирование по стилю проекта

#### 5.5.8 Обновление таблицы команд

После переименования `print_help_cb`:
```c
command_t cmd_list[] = {
    {
        .cmd  = "help",
        .help = "print available commands with their help text",
        .fn   = ucmd_help,
    },
    /* ... остальные команды ... */
    {0}, /* Null terminator — do not remove */
};
```

#### 5.5.9 Добавление Doxygen-комментариев

Все публичные функции получить комментарии по образцу из `ucmd.h` (раздел 5.4).

---

### 5.6 `term_gxf.h`

Без изменений. Атрибуция уже присутствует в существующем заголовке файла.

---

### 5.7 `README`

Добавить секцию о модификациях:

```
MODIFICATIONS FOR firmware-demo PROJECT
======================================
This copy of microrl has been significantly modified by Michael Kaa
for the firmware-demo project (2026). Key changes include:
- ESC sequence timeout handling (prevents parser state corruption)
- Code style alignment with project conventions
- Added precise_time.h integration for timing-based rate limiting
- Fixed uninitialized fields in microrl_init()

Original library credits remain with Eugene Samoylov (Helius).
```

---

## 6. Решение проблемы ESC-последовательностей

### 6.1 Описание проблемы

При быстрой последовательности вызовов `ucmd_default_proc()`, когда скорость вызовов превышает скорость поступления символов по UART, ESC-последовательности (например, `\033[A` для стрелки вверх) могут быть "разрезаны":

1. Приходит `\033` -> устанавливается `escape = 1`
2. Следующий вызов `proc()` не находит нового символа и обрабатывает мусор/старое значение как продолжение ESC
3. Парсер интерпретирует неверный символ -> теряется команда или ломается ввод

### 6.2 Архитектурное решение

**Двухуровневая защита:**

1. **Level 1: Rate limiting в ucmd_default_proc()** — через `precise_time.h`:
   - Ограничить частоту обработки до ~2 kHz (интервал 500 мкс)
   - Аналогично подходу в `pwm_led.c` (`pt_elapsed_us(led.last_stamp) < 1000`)

2. **Level 2: ESC-таймаут в microrl_insert_char()** — через `precise_time.h`:
   - При поступлении ESC установить метку времени `escape_stamp = pt_stamp()`
   - При каждом следующем символе проверить `pt_elapsed_us(escape_stamp) > _ESC_TIMEOUT_US`
   - Если таймаут exceeded — сбросить состояние escape и обработать символ как обычный

### 6.3 Диаграмма состояний ESC-парсера

```
          ESC приходит
              │
              ▼
        ┌─────────────┐
        │  escape=1    │←── установка escape_stamp
        │  waiting...  │
        └───────┬─────┘
                │
         ┌──────┴──────┐
         │             │
    символ           таймаут
    пришел          (>500 мкс)
         │             │
         ▼             ▼
   escape_process()  сброс escape=0
         │           обработка как
    ┌────┴────┐      обычный символ
    │         │
 завершён  ждём ещё
    │         │
    ▼         ▼
 сброс   продолжение
 escape  парсинга
```

### 6.4 Зависимости

Для реализации используются:
- `firmware/core/lib/time/precise_time.h` — функции `pt_stamp()`, `pt_elapsed_us()`
- Требуется предварительный вызов `pt_init()` в startup-коде

---

## 7. Таблица изменений API

| Функция / тип | Было | Стало | Совместимость |
|--------------|------|-------|---------------|
| `print_help_cb(int, char**)` | свободная функция | переименована в `ucmd_help(int, char**)` |Breaking — обновить таблицу команд |
| `ucmd_execute(int argc, char **argv)` | argv без const | `ucmd_execute(int argc, const char * const *argv)` | Breaking — согласование с microrl callback |
| структура `microrl_t` | полей escape_stamp нет | добавлено `uint32_t escape_stamp` (под `#ifdef _USE_ESC_SEQ`) | Binary compatible — поле в конце блока #ifdef |
| `ucmd_default_proc()` | блокирующий scanf + без rate limiting | неблокирующее чтение + rate limiting (500 мкс) | Compatible — сигнатура не изменилась |
| `_ESC_TIMEOUT_US` | не существует | новое макроопределение в config.h | N/A |

---

## 8. Порядок выполнения работ

| Этап | Описание | Файлы | Оценка сложности |
|------|----------|-------|-----------------|
| 1 | Атрибуция авторов | microrl.c, microrl.h, config.h, README | Низкая |
| 2 | Исправление ucmd.c | ucmd.c, ucmd.h | Средняя |
| 3 | Рефакторинг microrl (инициализация, stdlib) | microrl.c, microrl.h | Низкая |
| 4 | ESC-таймаут через precise_time.h | microrl.c, microrl.h, config.h | Средняя |
| 5 | Rate limiting в ucmd_default_proc() | ucmd.c | Средняя |
| 6 | Стилистика (Doxygen, нейминг) | все файлы модуля | Низкая |
| 7 | Сборка и проверка без варнингов | — | — |

---

## 9. Критерии приемки

Результат рефакторинга считается удовлетворительным, если выполнены все условия:

1. **Сборка** — проект собирается без предупреждений при `-Wall -Wextra -pedantic`
2. **Атрибуция** — все файлы на basis стороннего кода содержат заголовок с указанием оригинального автора
3. **Изоляция слоев** — `stm32f407xx.h` не подключается из файлов app-слоя (за исключением контролируемых исключений с явными комментариями)
4. **ESC-таймаут** — при отправке ESC-последовательностей с паузой > 500 мкс между символами состояние парсера корректно сбрасывается
5. **Rate limiting** — `ucmd_default_proc()` пропускает вызовы чаще 2 kHz без потери данных
6. **Стилистика** — все публичные API документированы через Doxygen, нейминг соответствует snake_case проекта
7. **Функциональность** — все существующие команды CLI продолжают работать (`help`, `reset`, `mem`, `time`, `led`, `w25q`, `usb`, `adc`)

---

## 10. Риски и меры снижения

| Риск | Вероятность | Влияние | Мера снижения |
|------|-----------|---------|--------------|
| Изменение сигнатуры callback нарушит другие модули CLI | Средняя | Высокое | Тщательная проверка всех extern объявлений |
| precise_time.h не инициализирован при первом вызове | Низкая | Критическое | Добавление assert/проверки в ucmd_default_init() |
| Rate limiting потеряет символы при пиковой нагрузке | Низкая | Среднее | Символы буферизуются драйвером UART — потери не будут |
| ESC-таймаут 500 мкс слишком мал/велик для медленного/быстрого UART | Средняя | Среднее | Значение настраивается через config.h, можно адаптировать |

---

## 11. Пасхалка: «GO SLEEP, STUPID USER!»

В функции `ucmd_execute()` присутствует сообщение-пасхалка, выводимое при повторном вводе
неизвестных команд (после 6 попыток):

```c
set_display_atrib(F_RED);
printf("GO SLEEP, STUPID USER!\r\n");
resetcolor();
```

**Происхождение:** строка является отсылкой к прошивке *Evo Reset Service* — сервисному меню
ZX-совместимого компьютера **ZX-Evolution (Pentevo)** от группы разработчиков NedoPC.
В оригинале сообщение выводилось как ироничная критическая ошибка при некорректных
действиях пользователя в интерфейсе.

Сохранился как локальный мем сообщества ретро-разработчиков и оставлен в коде намеренно.

**Источник:** <http://forum.nedopc.com/viewtopic.php?f=33&t=861&start=40>
**Источник:** <https://hermitlair.ucoz.com/blog/2017-07-14-937>

**Решение:** оставить без изменений — сообщение не влияет на функциональность и является
культурным артефактом проекта.
