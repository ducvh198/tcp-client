CC ?= gcc
CFLAGS ?= -Wall -Wextra -Werror -pedantic -std=c99 -O2
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L
PYTHON ?= python3
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=
LIBS ?=
TARGET ?= tcp-client

SRC_DIR = src

ifeq ($(TARGET),tcp-client.exe)
  BUILD_DIR = build_win
else
  BUILD_DIR = build_linux
endif

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/cli_args.c \
       $(SRC_DIR)/socket_client.c \
       $(SRC_DIR)/mode_interactive.c \
       $(SRC_DIR)/mode_oneshot.c \
       $(SRC_DIR)/signal_handler.c \
       $(SRC_DIR)/hex_utils.c \
       $(SRC_DIR)/hsm_decoder.c \
       $(SRC_DIR)/compat.c

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

.PHONY: all clean test win install uninstall

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -I$(SRC_DIR) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

win:
	$(MAKE) CC=x86_64-w64-mingw32-gcc TARGET=tcp-client.exe LIBS="-lws2_32" CPPFLAGS="" CFLAGS="-Wall -Wextra -std=c99 -O2"

test: all
	$(PYTHON) tests/test_runner.py

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

clean:
	rm -rf build build_linux build_win tcp-client tcp-client.exe

-include $(DEPS)
