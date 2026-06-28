<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright 2026 Michael Kaa -->

# Спецификация архитектурного рефакторинга модуля CLI

**Дата:** 2026-06-28
**Автор:** Michael Kaa
**Статус:** Утверждён (к реализации)

---

## 1. Контекст и мотивация

### 1.1 Назначение модуля

Модуль `firmware/cli/u_read_line/` реализует интерфейс командной строки (CLI) для встроенных систем на микроконтроллерах. Модуль платформонезависим и предназначен для использования в ресурсоограниченных embedded-средах. Обеспечивает:

- Чтение символов из потока ввода через абстрактный интерфейс (`drv_face_t`)
- Редактирование строки команд с навигацией курсора
- Историю команд, автодополнение (Tab)
- Парсинг и диспетчеризацию пользовательских команд

### 1.2 Проблемы текущей архитектуры

| # | Проблема | Влияние |
|---|----------|---------|
| P1 | Два монолита (`ucmd.c` + `microrl.c`) без чёткого разделения ответственности | Сложность поддержки, невозможно тестирование по частям |
| P2 | Таблица команд захаркождена внутри `ucmd.c` | Нарушение изоляции: модуль CLI знает о конкретных командах приложения |
| P3 | Используется `printf` вместо `drv_face_t->write()` | Невозможно мокировать вывод в тестах, нарушение единого канала IO |
| P4 | Ctrl+C обрабатывается через `void (*sigint)(void)` без контекста | Нет связи с прерванной командой, нет кодов возврата |
| P5 | Приложение, вызванное через CLI, не может перехватить proc | Невозможно создать интерактивные подкоманды (например, REPL для памяти) |
| P6 | Статический экземпляр `microrl_t default_rl` | Невозможно завести второй CLI на другом UART |
| P7 | Нет механизма аутентификации/авторизации | Любой доступ к UART = полный доступ ко всем командам |
| P8 | `term_gxf.h` использует `printf` в макросах | Зависимость от stdio, невозможно перенаправление вывода |
| P9 | Команда `reset` с прямым include CMSIS в `ucmd.c` | Нарушение изоляции слоев (контролируемое исключение, но требует выноса) |

### 1.3 Цели рефакторинга

После рефакторинга модуль CLI должен:

1. Поддерживать **множественные экземпляры** (один на каждый UART/USB CDC)
2. Принимать **внешнюю таблицу команд** при инициализации
3. Весь ввод-вывод вести **исключительно через `drv_face_t`**
4. Позволять приложениям **перехватывать proc** для интерактивного режима
5. Иметь **механизм аутентификации/авторизации** для управления доступом к CLI (вход/выход, уровни привилегий сессии)
6. Не зависеть от `printf`, CMSIS, term_gxf и других внешних монолитных библиотек
7. Собираться без предупреждений при `-Wall -Wextra -pedantic`

---

## 2. Раунд 1: Вынос зависимостей (декомпозиция)

### 2.1 Цели раунда

Разделить монолит `ucmd.c` на логические части, вынести зависимости во внешние модули. После этого раунда CLI остаётся одноэкземплярным, но структура кода становится подготовленной для архитектурного рефакторинга.

### 2.2 Вынос term_gxf.h в отдельную библиотеку

**Текущее состояние:** `firmware/cli/u_read_line/term_gxf.h` — заголовочный файл с макросами ANSI-escape, использующий `printf`.

**Целевое состояние:**
```
firmware/lib/termgfx/
└── termgfx.h  /* заголовочная библиотека (только хедер) */
```

Файл перемещается без изменений реализации. Макросы остаются макросами; переименование guard-макроса и префиксов — косметическое.

#### 2.2.1 Изменения

| Файл | Действие |
|------|----------|
| `firmware/cli/u_read_line/term_gxf.h` | Перемещён в `firmware/lib/termgfx/termgfx.h` |
| `firmware/Makefile` | Добавлен `-Ifirmware/lib/termgfx` в INCLUDES |

Все включения `#include "term_gxf.h"` заменяются на `#include "termgfx.h"`.

### 2.3 Вынос команды reset

**Текущее состояние:** Команда `ucmd_mcu_reset()` определена в `ucmd.c` с прямым include `<stm32f407xx.h>` и вызовом `NVIC_SystemReset()`.

**Целевое состояние:** Разделение на два слоя:

1. **Драйвер reset** (`firmware/core/drivers/reset/`, сабмодуль core) — платформозависимый код с CMSIS.
2. **CLI-команда** (`firmware/cli/reset/`, основной репозиторий) — обёртка над драйвером.

```
firmware/core/drivers/reset/   (сабмодуль core — слой драйверов)
├── reset.c  /* reset_perform() с NVIC_SystemReset */
└── reset.h  /* void reset_perform(void); */

firmware/cli/reset/            (основной репо — слой CLI)
├── ucmd_reset.c  /* ucmd_reset() вызывает reset_perform() */
└── ucmd_reset.h  /* int ucmd_reset(int argc, char *argv[]); */
```

```c
/* reset.h — в firmware/core/drivers/reset/ (сабмодуль core) */
#ifndef _RESET_H_
#define _RESET_H_

/** Выполнить сброс MCU (не возвращается) */
void reset_perform(void);

#endif /* _RESET_H_ */
```

```c
/* reset.c — в firmware/core/drivers/reset/ (сабмодуль core) */
#include "stm32f407xx.h"
#include "reset.h"

void reset_perform(void)
{
    NVIC_SystemReset();
}
```

```c
/* ucmd_reset.h — в firmware/cli/reset/ (основной репо) */
#ifndef _UCMD_RESET_H_
#define _UCMD_RESET_H_

int ucmd_reset(int argc, char *argv[]);

#endif /* _UCMD_RESET_H_ */
```

```c
/* ucmd_reset.c — в firmware/cli/reset/ (основной репо) */
#include "reset.h"  /* из core/drivers/reset/ */
#include "ucmd_reset.h"

int ucmd_reset(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    reset_perform();
    return 0; /* never reached */
}
```

В таблице команд CLI ссылка на `ucmd_reset`:
```c
{ .cmd = "reset", .help = "reset MCU", .fn = (command_cb)ucmd_reset },
```

### 2.4 Внешняя таблица команд

**Текущее состояние:** Статическая таблица `cmd_list[]` в `ucmd.c`.

**Целевое состояние:** Таблица команд определяется вне модуля CLI. При инициализации передаётся указатель на таблицу.

#### 2.4.1 Новый API (временный, для Раунда 1)

В `ucmd.h`:
```c
/**
 * @brief Инициализация CLI с внешней таблицей команд.
 * @param commands  Указатель на NULL-terminated массив command_t.
 */
void ucmd_default_init_with_commands(const command_t *commands);
```

#### 2.4.2 Сборка таблицы команд в app

Создаётся файл для сборки таблицы:
```
firmware/app/
├── cli_setup.c  /* сборка таблицы команд, инициализация CLI */
└── cli_setup.h  /* публичный API */
```

```c
/* cli_setup.h */
#ifndef _CLI_SETUP_H_
#define _CLI_SETUP_H_

void cli_setup_init(void);

#endif /* _CLI_SETUP_H_ */
```

Таблица команд в `cli_setup.c`:
```c
static command_t g_cli_commands[] = {
    { .cmd = "help",  .help = "print available commands", .fn = (command_cb)ucmd_help },
    { .cmd = "reset", .help = "reset MCU",                .fn = (command_cb)ucmd_reset },
    { .cmd = "mem",   .help = "memory manager",           .fn = (command_cb)ucmd_mem },
    { .cmd = "time",  .help = "RTC time control",         .fn = (command_cb)ucmd_time },
    { .cmd = "led",   .help = "LED control",              .fn = (command_cb)ucmd_led },
    { .cmd = "w25q",  .help = "W25Q flash control",       .fn = (command_cb)ucmd_w25q },
    { .cmd = "usb",   .help = "USB commands",             .fn = (command_cb)ucmd_usb },
    { .cmd = "adc",   .help = "ADC commands",             .fn = (command_cb)ucmd_adc },
    {0},  /* null terminator */
};

void cli_setup_init(void)
{
    ucmd_default_init_with_commands(g_cli_commands);
}
```

### 2.5 Изменения в kernel.c

Замена `ucmd_default_init()` на `cli_setup_init()`:
```c
/* Было: */
ucmd_default_init();

/* Стало: */
#include "cli_setup.h"
cli_setup_init();
```

### 2.6 Критерии приемки Раунда 1

| # | Критерий | Проверка |
|---|----------|----------|
| A1.1 | Сборка без предупреждений (`-Wall -Wextra -pedantic`) | `make all` |
| A1.2 | Все команды CLI работают (`help`, `reset`, `mem`, `time`, `led`, `w25q`, `usb`, `adc`) | Тест на железе |
| A1.3 | `term_gxf.h` удалён из `u_read_line/` | Проверка файлов |
| A1.4 | `termgfx/` создан в `firmware/lib/` | Проверка файлов |
| A1.5 | Команда reset вынесена в `firmware/core/drivers/reset/` | Проверка файлов |
| A1.6 | Таблица команд определена вне `ucmd.c` | Проверка кода |
| A1.7 | Прямой include CMSIS из `ucmd.c` удалён | Проверка кода |
| A1.8 | Регрессия на железе: все функции работают как до рефакторинга | Тест на железе |

---

## 3. Раунд 2: Экземплярное ядро (архитектурный рефакторинг)

### 3.1 Цели раунда

Превратить CLI из одноэкземплярного монолита в многэкземплярную библиотеку с полным разделением слоев.

### 3.2 Структура cli_t

```c
/* cli.h */
#ifndef _CLI_H_
#define _CLI_H_

#include <stddef.h>
#include <stdint.h>
#include "ucmd.h"       /* command_t, command_cb */
#include "drv_face.h"   /* drv_face_t */

/* Уровни привилегий команд */
typedef enum {
    CLI_PRIV_PUBLIC   = 0,   /* доступна всем без ограничений */
    CLI_PRIV_OPERATOR = 1,   /* требует авторизации оператора */
    CLI_PRIV_ADMIN    = 2,   /* требует авторизации администратора */
    CLI_PRIV_DISABLED = 0xFF /* команда отключена (не выполняется) */
} cli_privilege_t;

/* Расширенная структура команды (с уровнем привилегий) */
typedef struct {
    const char      *cmd;              /* строка команды           */
    const char      *help;             /* текст помощи             */
    command_cb       fn;               /* функция-обработчик       */
    cli_privilege_t  privilege_level;  /* требуемый уровень прав   */
} cli_command_t;

/* Результат проверки аутентификации */
typedef enum {
    CLI_AUTH_OK         =  0, /* команда разрешена     */
    CLI_AUTH_DENIED    = -1,  /* команда запрещена     */
    CLI_AUTH_NEED_LOGIN= -2,  /* требуется вход        */
} cli_auth_result_t;

/* Callback для проверки аутентификации перед выполнением команды */
typedef cli_auth_result_t (*cli_auth_callback_t)(
    const char *command,
    cli_privilege_t required_level,
    void *user_ctx);

/*
 * Callback для перехвата proc. Позволяет приложению взять управление
 * на себя (интерактивный режим). При выходе приложение устанавливает
 * handler = NULL для возврата к стандартному CLI.
 */
typedef void (*cli_proc_handler_t)(cli_t *cli);

/* Информация о прерывании Ctrl+C */
typedef struct {
    const char      *interrupted_cmd; /* команда, которая была прервана */
    int              argc;            /* аргументы команды               */
    const char     **argv;
    void           *user_ctx;         /* контекст приложения             */
} cli_sigint_info_t;

typedef void (*cli_sigint_callback_t)(cli_sigint_info_t *info);

/* Конфигурация экземпляра CLI */
typedef struct {
    const drv_face_t  *iface;            /* интерфейс ввода-вывода     */
    const cli_command_t *commands;       /* таблица команд (внешняя)   */
    const char        *prompt;           /* промпт (NULL = дефолт)     */
    cli_auth_callback_t auth_cb;         /* хук аутентификации (NULL = без проверки) */
    void              *auth_user_ctx;    /* контекст для auth_cb       */
    cli_sigint_callback_t sigint_cb;     /* обработчик Ctrl+C          */
} cli_config_t;

/* Неполный тип экземпляра CLI */
typedef struct cli cli_t;

/*
 * Создание экземпляра CLI.
 * @param config  Конфигурация (живёт до cli_destroy или копируется).
 * @param mem     Память для экземпляра (выделена приложением, без malloc).
 * @param mem_size Размер выделенной памяти.
 * @return Указатель на экземпляр или NULL при ошибке.
 */
cli_t *cli_create(const cli_config_t *config, void *mem, size_t mem_size);

/* Уничтожение экземпляра */
void cli_destroy(cli_t *cli);

/*
 * Обработка одного цикла (вызывать из main loop).
 * Читает данные из iface, обрабатывает ввод, выполняет команды.
 */
void cli_proc(cli_t *cli);

/*
 * Установка/снятие перехватчика proc.
 * @param handler  NULL = вернуть стандартное поведение CLI.
 */
void cli_set_proc_handler(cli_t *cli, cli_proc_handler_t handler);

/*
 * Получение интерфейса drv_face_t из экземпляра (для использования
 * интерактивным приложением).
 */
const drv_face_t *cli_get_iface(const cli_t *cli);

/*
 * Вывод строки через интерфейс экземпляра CLI.
 * Предоставляется интерактивным приложениям для вывода данных.
 */
void cli_output(cli_t *cli, const char *str);

/*
 * Вывод форматированной строки через интерфейс экземпляра CLI.
 */
void cli_outputf(cli_t *cli, const char *fmt, ...);

/*
 * Получение текущего уровня привилегий сессии.
 */
cli_privilege_t cli_get_session_privilege(const cli_t *cli);

/*
 * Установка уровня привилегий сессии (вызывается из кода аутентификации).
 */
void cli_set_session_privilege(cli_t *cli, cli_privilege_t level);

#endif /* _CLI_H_ */
```

### 3.3 Замена printf на drv_face_t->write()

#### 3.3.1 Что меняется

| Файл | Было | Стало |
|------|------|-------|
| `ucmd.c` / `cli.c` | `printf(...)` | `cli_output(cli, ...)` через snprintf + iface->write |
| `microrl.c` | `pThis->print()` (delegate в printf) | `pThis->print()` -> cli->iface->write |
| `ucmd_help()` | `printf("%s \t%s\r\n", ...)` | `cli_outputf(cli, ...)` |
| `ucmd_execute()` | printf для "unknown command" | `cli_outputf(cli, ...)` |
| `termgfx.h` | макросы с `printf` | Не меняется в Раунде 2 (используется командами CLI при необходимости) |

#### 3.3.2 Буферизация вывода

Внутри `cli_t` выделяется статический буфер для форматированного вывода:

```c
#define CLI_TX_BUF_SIZE 256

struct cli {
    microrl_t          rl;             /* readline-контекст            */
    const drv_face_t  *iface;          /* интерфейс ввода-вывода       */
    const cli_command_t *commands;     /* таблица команд               */
    char               tx_buf[CLI_TX_BUF_SIZE]; /* буфер вывода        */
    cli_privilege_t    session_priv;   /* текущий уровень привилегий   */
    cli_auth_callback_t auth_cb;       /* хук аутентификации           */
    void              *auth_user_ctx;  /* контекст для auth_cb         */
    cli_proc_handler_t proc_handler;   /* перехватчик proc (NULL)      */
    cli_sigint_callback_t sigint_cb;   /* обработчик Ctrl+C            */
    /* ... внутренние поля ... */
};
```

Функция `cli_outputf`:
```c
void cli_outputf(cli_t *cli, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(cli->tx_buf, sizeof(cli->tx_buf), fmt, args);
    va_end(args);
    if (len > 0 && len < (int)sizeof(cli->tx_buf)) {
        cli->iface->write(cli->tx_buf, (size_t)len);
    }
}
```

### 3.4 Механизм перехвата proc

#### 3.4.1 Принцип работы

```
+------------------+     +---------------------+
|  cli_proc(cli)   |     |  cli_proc(cli)      |
+------------------+     +---------------------+
| proc_handler ==  |     | proc_handler != NULL|
| NULL             |     |                     |
+------------------+     +---------------------+
| 1. чтение из     |     | 1. вызов             |
|    iface->read() |     |    proc_handler(cli) |
| 2. microrl_      |     |                      |
|    insert_char() |     | Приложение само:     |
| 3. обработка     |     | - читает из          |
|    команд        |     |   cli_get_iface()    |
+------------------+     | - выводит через      |
                         |   cli_output()       |
                         | - при завершении     |
                         |   устанавливает      |
                         |   handler = NULL     |
                         +---------------------+
```

#### 3.4.2 Реализация

В `cli_proc`:
```c
void cli_proc(cli_t *cli)
{
    if (cli->proc_handler != NULL) {
        /* Приложение перехватило управление */
        cli->proc_handler(cli);
        return;
    }

    /* Стандартное поведение CLI */
    uint8_t ch;
    int ret = cli->iface->read(&ch, 1);
    if (ret <= 0) return;
    microrl_insert_char(&cli->rl, ch);
}
```

#### 3.4.3 Пример использования (интерактивное приложение)

```c
/* Глобальное состояние интерактивной сессии */
static cli_t *g_interactive_cli = NULL;
static int    g_session_active   = 0;

/* Обработчик команды "mem" — переход в интерактивный режим */
static int mem_command_interactive(int argc, char *argv[])
{
    (void)argc; (void)argv;

    /* Сохранить указатель на CLI */
    g_interactive_cli = current_cli;  /* передано каким-то образом */
    g_session_active = 1;

    /* Перехватить proc */
    cli_set_proc_handler(g_interactive_cli, mem_interactive_proc);

    cli_outputf(g_interactive_cli, "Entering memory interactive mode...\r\n");
    cli_outputf(g_interactive_cli, "Type 'exit' to return.\r\n");

    return 0;
}

/* Интерактивный обработчик proc */
static void mem_interactive_proc(cli_t *cli)
{
    uint8_t ch;
    int ret = cli_get_iface(cli)->read(&ch, 1);
    if (ret <= 0) return;

    /* ... обработка ввода приложения ... */

    if (/* условие выхода */) {
        g_session_active = 0;
        cli_set_proc_handler(cli, NULL); /* вернуть управление CLI */
        cli_outputf(cli, "\r\nReturned to CLI.\r\n");
    }
}
```

### 3.5 Ctrl+C с контекстом

#### 3.5.1 Изменения в microrl

При нажатии Ctrl+C вызывается callback с информацией:

```c
/* Внутренний обработчик в cli.c */
static void cli_on_sigint(cli_t *cli)
{
    if (cli->sigint_cb == NULL) {
        cli_outputf(cli, "^C\r\n");
        return;
    }

    cli_sigint_info_t info;
    info.interrupted_cmd = cli->last_command; /* если известно */
    info.argc            = 0;
    info.argv            = NULL;
    info.user_ctx        = cli->auth_user_ctx;

    cli->sigint_cb(&info);
}
```

#### 3.5.2 Поведение при перехвате proc

Когда `proc_handler != NULL`, Ctrl+C обрабатывается самим приложением (через обычный поток ввода). CLI не перехватывает его.

### 3.6 Аутентификация и авторизация

#### 3.6.1 Модель доступа

Аутентификация управляет доступом к CLI на уровне сессии, а не только перед выполнением отдельных команд. При подключении к терминалу сессия начинает с минимального уровня привилегий (`CLI_PRIV_PUBLIC`). Для выполнения команд, требующих более высокий уровень, пользователь должен пройти аутентификацию.

#### 3.6.2 Механизм проверки

Перед выполнением каждой команды CLI проверяет:
1. Требуемый уровень привилегий команды (`cmd->privilege_level`)
2. Текущий уровень привилегий сессии (`cli->session_priv`)
3. Если `required > session` -> вызывается `auth_cb`

```c
static int cli_execute_command(cli_t *cli, int argc, const char *const *argv)
{
    /* Поиск команды в таблице */
    const cli_command_t *cmd = cli_find_command(cli, argv[0]);
    if (!cmd) {
        cli_outputf(cli, "unknown command %s\r\n", argv[0]);
        return UCMD_CMD_NOT_FOUND;
    }

    /* Проверка: команда отключена? */
    if (cmd->privilege_level == CLI_PRIV_DISABLED) {
        cli_outputf(cli, "command '%s' is disabled\r\n", cmd->cmd);
        return -EPERM;
    }

    /* Проверка привилегий */
    if (cmd->privilege_level > cli->session_priv) {
        if (cli->auth_cb != NULL) {
            cli_auth_result_t result = cli->auth_cb(
                cmd->cmd, cmd->privilege_level, cli->auth_user_ctx);

            switch (result) {
                case CLI_AUTH_OK:
                    /* Аутентификация прошла, уровень обновлён callback-ом */
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
            /* Нет callback аутентификации — отказ */
            cli_outputf(cli, "access denied: '%s' (no auth provider)\r\n", cmd->cmd);
            return -EPERM;
        }
    }

    /* Выполнение команды */
    return cmd->fn(argc, (char **)argv);
}
```

#### 3.6.3 Выход из сессии (logout)

CLI поддерживает явный выход из аутентифицированной сессии:
- Команда `logout` (или аналогичный механизм) сбрасывает уровень привилегий до `CLI_PRIV_PUBLIC`.
- Состояние аутентификации хранится в пользовательском контексте (`auth_user_ctx`), который управляет приложение.

#### 3.6.4 TODO: Таймаут неактивности (будущая версия)

> **Примечание:** На текущем этапе механизм автоматического разлогина по таймауту не реализуется. Это требует:
> - Трекинга времени последней активности сессии в `cli_t`.
> - Периодической проверки таймаута в `cli_proc()`.
> - Настройки таймаута через `cli_config_t`.
> - Уведомления пользователя перед разлогином.
>
> Зафиксировано как задача для будущего цикла разработки.

#### 3.6.5 Пример реализации аутентификации (в app)

```c
/* Состояние сессии */
typedef struct {
    cli_privilege_t logged_in_level;
    int             authenticated;
} session_state_t;

static session_state_t g_session = { .logged_in_level = CLI_PRIV_PUBLIC };

static cli_auth_result_t auth_check(
    const char *command,
    cli_privilege_t required_level,
    void *user_ctx)
{
    (void)command;
    (void)user_ctx;

    if (g_session.authenticated &&
        g_session.logged_in_level >= required_level) {
        return CLI_AUTH_OK;
    }

    /* Запросить пароль через CLI */
    cli_output("Login: ");
    /* ... чтение логина/пароля ... */

    if (/* проверка успешна */) {
        g_session.authenticated = 1;
        g_session.logged_in_level = required_level;
        /* Обновить уровень в CLI */
        // cli_set_session_privilege(cli, required_level);
        return CLI_AUTH_OK;
    }

    return CLI_AUTH_DENIED;
}
```

### 3.7 Множественные экземпляры CLI

#### 3.7.1 Использование

```c
/* В kernel.c или app */
#include "cli.h"

/* Память для экземпляров (статическая, без malloc) */
static char cli_uart1_mem[sizeof(cli_t) + some_padding];
static char cli_uart2_mem[sizeof(cli_t) + some_padding];

/* Или точнее — определить максимальный размер */
#define CLI_INSTANCE_SIZE 1024
static uint8_t cli_uart1_mem[CLI_INSTANCE_SIZE];
static uint8_t cli_uart2_mem[CLI_INSTANCE_SIZE];

void app_cli_init(void)
{
    /* Таблица команд для UART1 */
    static const cli_command_t cmds_uart1[] = { ... };

    /* Таблица команд для UART2 (может быть другой набор!) */
    static const cli_command_t cmds_uart2[] = { ... };

    /* Экземпляр 1 — UART1 */
    cli_config_t cfg1 = {
        .iface     = dev_uart1_get(),
        .commands  = cmds_uart1,
        .prompt    = "UART1> ",
        .auth_cb   = auth_check,
        .auth_user_ctx = &g_session,
    };
    cli_t *cli1 = cli_create(&cfg1, cli_uart1_mem, sizeof(cli_uart1_mem));

    /* Экземпляр 2 — UART2 */
    cli_config_t cfg2 = {
        .iface     = dev_uart2_get(),
        .commands  = cmds_uart2,
        .prompt    = "UART2> ",
    };
    cli_t *cli2 = cli_create(&cfg2, cli_uart2_mem, sizeof(cli_uart2_mem));

    /* В main loop: */
    /* cli_proc(cli1); */
    /* cli_proc(cli2); */
}
```

### 3.8 Обновление kernel.c

После Раунда 2 `kernel.c` выглядит так:
```c
#include "cli.h"
#include "cli_setup.h"

int main(void)
{
    /* ... инициализация периферии ... */

    /* Создание экземпляра CLI */
    cli_config_t cfg = cli_setup_get_config();
    g_cli = cli_create(&cfg, g_cli_mem, sizeof(g_cli_mem));

    while (1) {
        cli_proc(g_cli);
        led_proc();
        printf_flush();
    }
}
```

### 3.9 Критерии приемки Раунда 2

| # | Критерий | Проверка |
|---|----------|----------|
| A2.1 | Сборка без предупреждений (`-Wall -Wextra -pedantic`) | `make all` |
| A2.2 | Все команды CLI работают | Тест на железе |
| A2.3 | Нет вызовов printf в модуле CLI | Статический анализ кода |
| A2.4 | Создание двух экземпляров CLI (UART1 + UART2) | Тест на железе |
| A2.5 | Перехват proc приложением работает | Тест интерактивной команды |
| A2.6 | Хук аутентификации вызывается перед командами с привилегиями | Тест на железе |
| A2.7 | Ctrl+C передаёт контекст прерванной команды | Тест на железе |
| A2.8 | Unit-тесты проходят (обновлённые) | `make test` |
| A2.9 | Регрессия на железе: все функции работают как до рефакторинга | Тест на железе |

---

## 4. Структура файлов после рефакторинга

### 4.1 До (текущее состояние)

```
firmware/cli/u_read_line/
├── config.h
├── microrl.c
├── microrl.h
├── README
├── term_gxf.h
├── ucmd.c
└── ucmd.h
```

### 4.2 После Раунда 1

```
firmware/cli/u_read_line/
├── config.h
├── microrl.c
├── microrl.h
├── README
├── ucmd.c          /* без таблицы команд, без reset, без term_gxf */
└── ucmd.h

firmware/core/drivers/reset/   (сабмодуль core)
├── reset.c  /* reset_perform() — платформозависимый */
└── reset.h

firmware/cli/reset/            (основной репо)
├── ucmd_reset.c  /* ucmd_reset() — обёртка над драйвером */
└── ucmd_reset.h

firmware/lib/termgfx/
└── termgfx.h

firmware/app/
├── cli_setup.c     /* таблица команд + инициализация */
└── cli_setup.h
```

### 4.3 После Раунда 2

```
firmware/cli/u_read_line/
├── config.h
├── microrl.c       /* чистый readline, без printf */
├── microrl.h
├── cli.c           /* новое ядро CLI (экземплярное) */
├── cli.h           /* публичный API */
├── ucmd.h          /* оставлено для command_t / устаревший API, постепенно убрать */
└── README

firmware/core/drivers/reset/   (сабмодуль core)
├── reset.c
└── reset.h

firmware/cli/reset/            (основной репо)
├── ucmd_reset.c
└── ucmd_reset.h

firmware/lib/termgfx/
└── termgfx.h

firmware/app/
├── cli_setup.c     /* конфигурация экземпляров CLI */
├── cli_setup.h
├── cli_auth.c      /* реализация аутентификации (опционально) */
└── cli_auth.h
```

---

## 5. План тестирования

### 5.1 После каждого этапа Раунда 1

| Этап | Тест на железе | Описание |
|------|----------------|----------|
| 2.2 (termgfx) | Да | Проверить, что цветовые сообщения и ANSI-escape работают |
| 2.3 (reset) | Да | Команда reset работает после выноса в подмодуль |
| 2.4 (внешняя таблица) | Да | Все команды выполняются при внешней таблице |
| Конец Раунда 1 | Полная регрессия | Все функции системы работают как до рефакторинга |

### 5.2 После каждого этапа Раунда 2

| Этап | Тест на железе | Описание |
|------|----------------|----------|
| 3.3 (printf -> write) | Да | Вывод CLI корректен, нет артефактов |
| 3.4 (перехват proc) | Да | Интерактивная команда захватывает и отдаёт управление |
| 3.5 (Ctrl+C с контекстом) | Да | Ctrl+C корректно обрабатывается с информацией |
| 3.6 (аутентификация) | Да | Команды с привилегиями требуют авторизацию |
| 3.7 (множественные экземпляры) | Да | Два CLI на UART1 и UART2 работают независимо |
| Конец Раунда 2 | Полная регрессия | Все функции системы работают как до рефакторинга |

### 5.3 Обновление unit-тестов

После каждого раунда обновляются тесты в `firmware/tests/cli/u_read_line/`:
- Мокирование `drv_face_t` для проверки вывода через write()
- Тесты экземпляров CLI (создание, уничтожение, proc_handler)
- Тесты аутентификации (проверка callback вызова)

---

## 6. Риски и меры снижения

| Риск | Вероятность | Влияние | Мера снижения |
|------|-----------|---------|--------------|
| Буфер вывода 256 байт мал для длинных сообщений | Средняя | Среднее | Настройка через config.h, проверка переполнения |
| Перехват proc приведёт к блокировке CLI при зависании приложения | Низкая | Высокое | Таймаут возврата в стандартный режим (опционально) |
| Изменение command_t -> cli_command_t нарушит подмодули CLI | Высокая | Среднее | Сохранить backward-совместимость через обёртку или этап миграции |
| Потеря данных при множественных экземплярах из-за общих статических буферов в microrl | Средняя | Высокое | Проверка всех статических буферов в microrl.c на потокобезопасность |
| Регрессия после замены printf на write | Низкая | Среднее | Тестирование на железе после каждого шага |

---

## 7. Глоссарий

| Термин | Описание |
|--------|----------|
| CLI | Command Line Interface — интерфейс командной строки |
| drv_face_t | Общий интерфейс устройства (read/write/ioctl) |
| microrl | Readline-библиотека для embedded-систем |
| proc | Функция обработки, вызываемая в main loop |
| proc_handler | Перехватчик proc для интерактивных приложений |
| auth_cb | Callback проверки аутентификации/авторизации |
| termgfx | Библиотека ANSI-escape графики терминала |

---

## 8. Атрибуция авторов

### 8.1 Библиотека microrl

Ядро readline-функциональности основано на библиотеке **microrl**, созданной:

- **Автор:** Eugene Samoylov (Helius) \<ghelius@gmail.com\>
- **Оригинальный репозиторий:** <https://github.com/helius/microrl>
- **Модификации для firmware-demo:** Michael Kaa
- **Изменения:** ESC-таймаут, замена scanf на drv_face_t, инициализация полей, стилизация кода

Во все файлы, происходящие из microrl (`microrl.c`, `microrl.h`, `config.h`), уже добавлен заголовок атрибуции (сделано в первом рефакторинге, см. `u_read_line_refactoring.md`).

### 8.2 Терминальная графика (termgfx)

Идея ANSI-escape макросов для терминала принадлежит:

- **Автор идеи:** Zefick
- **Статья:** «Терминальная графика» (Когда printf — мало, а ncurses — много)
- **Платформа:** Habr
- **Дата публикации:** 28 марта 2017 г.
- **Ссылка:** <https://habr.com/ru/articles/325082/>

В `termgfx.h` сохранить комментарий о первоисточнике.

### 8.3 Пасхалка: «GO SLEEP, STUPID USER!»

Сообщение в функции обработки неизвестных команд — отсылка к прошивке *Evo Reset Service* для ZX-совместимого компьютера **ZX-Evolution (Pentevo)** от NedoPC.

- **Источник (форум):** <http://forum.nedopc.com/viewtopic.php?f=33&t=861&start=40>
- **Источник (блог):** <https://hermitlair.ucoz.com/blog/2017-07-14-937>

**Решение:** оставить без изменений — культурный артефакт проекта.

### 8.4 Собственный код

Остальные компоненты модуля CLI (`cli.c`, `cli.h`, `ucmd.c/h` после рефакторинга, `cli_setup.c/h`) являются оригинальной разработкой:

- **Автор:** Michael Kaa
- **Лицензия:** Apache-2.0 (согласно `.gitattributes` проекта)

---

## 9. Обновление README (финальный этап)

После завершения обоих раундов рефакторинга обновляется файл `firmware/cli/u_read_line/README`:

### 9.1 Что добавить

| Раздел readme | Содержание |
|---------------|-----------|
| Обзор модуля | Описание новой архитектуры (экземплярная модель, cli_t) |
| Зависимости | `drv_face_t`, `termgfx`, `precise_time` |
| API | Краткое описание публичных функций из `cli.h` |
| Множественные экземпляры | Пример создания двух CLI на разных UART |
| Перехват proc | Пример интерактивного приложения |
| Аутентификация | Описание хука и уровней привилегий |
| Атрибуция | Ссылки на авторов (microrl, termgfx) |
| Изменения относительно оригинала | Перечень модификаций microrl для firmware-demo |

### 9.2 Что убрать

- Устаревшие ссылки на `ucmd_default_init()` / `ucmd_default_proc()` как на основной API
- Описание внутренней реализации, которая больше не актуальна
- Ссылки на удалённые файлы (`term_gxf.h`)

---

## 10. Ссылки

- Спецификация первого рефакторинга: `u_read_line_refactoring.md`
- Спецификация unit-тестов: `u_read_line_tests.md`
- Правила проекта: `.clinerules`
