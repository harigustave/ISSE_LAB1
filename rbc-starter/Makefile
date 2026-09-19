UNAME_S := $(shell uname -s)

# Use the course compiler on Linux and Apple Clang on macOS. A command-line
# CC=... override still takes precedence.
ifeq ($(origin CC), default)
ifeq ($(UNAME_S),Darwin)
CC := clang
else
CC := gcc
endif
endif

# Homebrew installs Criterion outside Apple Clang's default search paths.
# Prefer pkg-config metadata when available, while retaining the ordinary
# -lcriterion fallback used by standard Linux/Codespaces installations.
PKG_CONFIG ?= pkg-config
CRITERION_CFLAGS := $(shell $(PKG_CONFIG) --cflags criterion 2>/dev/null)
CRITERION_LIBS := $(shell $(PKG_CONFIG) --libs criterion 2>/dev/null)
ifeq ($(strip $(CRITERION_LIBS)),)
CRITERION_LIBS := -lcriterion
endif

CPPFLAGS := -Iinclude
WARNINGS := -Wall -Wextra -Wpedantic -Wstrict-prototypes -Wmissing-prototypes
CFLAGS_COMMON := -std=c17 $(WARNINGS) -g3 -O0 -MMD -MP
TEST_CPPFLAGS := -Iinclude -Itests/support
TEST_DEFINES := -DRBC_TESTING
SAN_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined -g3 -O0

PROD_SRCS := \
	src/input.c \
	src/lexer.c \
	src/ast.c \
	src/parser.c \
	src/eval.c \
	src/alloc.c \
	src/main.c

LIB_SRCS := \
	src/input.c \
	src/lexer.c \
	src/ast.c \
	src/parser.c \
	src/eval.c \
	src/alloc.c

UNIT_SRCS := \
	tests/unit/test_input.c \
	tests/unit/test_lexer.c \
	tests/unit/test_ast.c \
	tests/unit/test_parser.c \
	tests/unit/test_eval.c

NORMAL_OBJS := $(patsubst src/%.c,build/normal/obj/%.o,$(PROD_SRCS))
TEST_SRC_OBJS := $(patsubst src/%.c,build/test/obj/src/%.o,$(LIB_SRCS))
TEST_UNIT_OBJS := $(patsubst tests/unit/%.c,build/test/obj/unit/%.o,$(UNIT_SRCS))
TEST_DIAG_OBJ := build/test/obj/diagnostic/resource_probe.o

SAN_PROD_OBJS := $(patsubst src/%.c,build/sanitize/obj/prod/%.o,$(PROD_SRCS))
SAN_TEST_SRC_OBJS := $(patsubst src/%.c,build/sanitize/obj/test/src/%.o,$(LIB_SRCS))
SAN_TEST_UNIT_OBJS := $(patsubst tests/unit/%.c,build/sanitize/obj/test/unit/%.o,$(UNIT_SRCS))

NORMAL_BIN := build/normal/bin/rbc
TEST_BIN := build/test/bin/rbc_tests
RESOURCE_PROBE := build/test/bin/rbc_resource_probe
SAN_BIN := build/sanitize/bin/rbc
SAN_TEST_BIN := build/sanitize/bin/rbc_tests

ALL_DEPS := \
	$(NORMAL_OBJS:.o=.d) \
	$(TEST_SRC_OBJS:.o=.d) \
	$(TEST_UNIT_OBJS:.o=.d) \
	$(TEST_DIAG_OBJ:.o=.d) \
	$(SAN_PROD_OBJS:.o=.d) \
	$(SAN_TEST_SRC_OBJS:.o=.d) \
	$(SAN_TEST_UNIT_OBJS:.o=.d)

.PHONY: all test sanitize valgrind clean

all: $(NORMAL_BIN)

$(NORMAL_BIN): $(NORMAL_OBJS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_COMMON) $^ -o $@

build/normal/obj/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS_COMMON) -c $< -o $@

$(TEST_BIN): $(TEST_SRC_OBJS) $(TEST_UNIT_OBJS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_COMMON) $^ $(CRITERION_LIBS) -o $@

build/test/obj/src/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CPPFLAGS) $(TEST_DEFINES) $(CFLAGS_COMMON) -c $< -o $@

build/test/obj/unit/%.o: tests/unit/%.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CPPFLAGS) $(CRITERION_CFLAGS) $(TEST_DEFINES) $(CFLAGS_COMMON) -c $< -o $@

$(TEST_DIAG_OBJ): tests/diagnostic/resource_probe.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CPPFLAGS) $(TEST_DEFINES) $(CFLAGS_COMMON) -c $< -o $@

$(RESOURCE_PROBE): \
	build/test/obj/src/lexer.o \
	build/test/obj/src/ast.o \
	build/test/obj/src/parser.o \
	build/test/obj/src/alloc.o \
	$(TEST_DIAG_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_COMMON) $^ -o $@

.PHONY: test
test: $(TEST_BIN) $(NORMAL_BIN)
	$(TEST_BIN)
	tests/cli/test_cli.sh $(NORMAL_BIN)

$(SAN_BIN): $(SAN_PROD_OBJS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_COMMON) $(SAN_FLAGS) $^ -o $@

build/sanitize/obj/prod/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS_COMMON) $(SAN_FLAGS) -c $< -o $@

$(SAN_TEST_BIN): $(SAN_TEST_SRC_OBJS) $(SAN_TEST_UNIT_OBJS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_COMMON) $(SAN_FLAGS) $^ $(CRITERION_LIBS) -o $@

build/sanitize/obj/test/src/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CPPFLAGS) $(TEST_DEFINES) $(CFLAGS_COMMON) $(SAN_FLAGS) -c $< -o $@

build/sanitize/obj/test/unit/%.o: tests/unit/%.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CPPFLAGS) $(CRITERION_CFLAGS) $(TEST_DEFINES) $(CFLAGS_COMMON) $(SAN_FLAGS) -c $< -o $@

.PHONY: sanitize
sanitize: $(SAN_TEST_BIN) $(SAN_BIN)
	ASAN_OPTIONS=detect_leaks=0 $(SAN_TEST_BIN)
	ASAN_OPTIONS=detect_leaks=0 $(SAN_BIN) < tests/fixtures/sanitize.in

.PHONY: valgrind
valgrind: $(RESOURCE_PROBE)
	valgrind --leak-check=full --show-leak-kinds=all \
		--errors-for-leak-kinds=all --error-exitcode=99 $(RESOURCE_PROBE)

.PHONY: clean
clean:
	rm -rf build

-include $(ALL_DEPS)
