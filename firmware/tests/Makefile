# Makefile для юнит-тестов firmware-demo (хост-компиляция, gcc x86_64)
#
# Использование:
#   make          - собрать и запустить все тесты
#   make clean    - удалить артефакты

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -Werror
CFLAGS += -g -O0
CFLAGS += -D_DEFAULT_SOURCE

# Unity (сабмодуль в tests/unity)
UNITY_DIR = unity/src

# Пути к стубам и тестируемым модулям
STUBS_DIR    = test_helpers

# ------------------------------------------------------------------
# Include-пути
# ВАЖНО: test_helpers/ идёт первым, чтобы cmsis_stubs.h перекрыл
# оригинальный stm32f407xx.h из CMSIS.
# ------------------------------------------------------------------
INCLUDES = -I$(STUBS_DIR)
INCLUDES += -I$(UNITY_DIR)

# Пути к модулям из основного репо и сабмодуля core
CORE_KERNEL   = ../core/kernel
CORE_LIB_TIME = ../core/lib/time
CLI_UREADLINE = ../cli/u_read_line

INCLUDES += -I$(CORE_KERNEL)
INCLUDES += -I$(CORE_LIB_TIME)
INCLUDES += -I$(CLI_UREADLINE)

# ------------------------------------------------------------------
# CLI-тесты (u_read_line)
# ------------------------------------------------------------------

CLI_DIR      = cli/u_read_line
CLI_SRCS = $(STUBS_DIR)/stubs.c \
           $(UNITY_DIR)/unity.c \
           $(STUBS_DIR)/mock_drv_face.c \
           $(STUBS_DIR)/ucmd_stub.c \
           $(CLI_UREADLINE)/microrl.c \
           $(CLI_DIR)/test_all.c

# Принудительно включить cmsis_stubs.h для ВСЕХ файлов (нужен для precise_time.h)
CLI_CFLAGS += -include $(STUBS_DIR)/cmsis_stubs.h

TARGET_CLI = test_cli_runner

# ------------------------------------------------------------------
# Правила
# ------------------------------------------------------------------

all: $(TARGET_CLI)
	@echo ""
	@echo "=== Запуск тестов cli ==="
	./$(TARGET_CLI)

$(TARGET_CLI): $(CLI_SRCS)
	$(CC) $(CFLAGS) $(CLI_CFLAGS) $(INCLUDES) $^ -o $@

clean:
	rm -f $(TARGET_CLI)

.PHONY: all clean

test_cli: $(TARGET_CLI)
	@echo ""
	@echo "=== Запуск тестов cli ==="
	./$(TARGET_CLI)