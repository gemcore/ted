# Makefile - host-side build and unit tests for the ted editor modules.
#
# The editor sources in src/ have no OS dependencies (fs_lfs.c uses stdio
# on a host and the Zephyr filesystem API when built with __ZEPHYR__), so
# they can be compiled and tested with a plain C toolchain:
#
#   make          # build the test binaries
#   make test     # build and run all unit tests
#   make clean

CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -Werror -Iinclude -g
BUILD   := build

CORE_SRCS := src/editor_store.c src/editor_core.c src/editor_view.c src/term_vt100.c \
             src/fs_lfs.c src/editor_session.c src/cmd_edit.c

TESTS := test_editor_core test_editor_store test_editor_view test_term_vt100 \
         test_editor_session

TEST_BINS := $(addprefix $(BUILD)/,$(TESTS))

.PHONY: all test clean

all: $(TEST_BINS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_editor_core: tests/test_editor_core.c src/editor_store.c src/editor_core.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/test_editor_store: tests/test_editor_store.c src/editor_store.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/test_editor_view: tests/test_editor_view.c src/editor_store.c src/editor_core.c src/editor_view.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/test_term_vt100: tests/test_term_vt100.c src/editor_store.c src/editor_core.c src/editor_view.c src/term_vt100.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/test_editor_session: tests/test_editor_session.c $(CORE_SRCS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^

test: $(TEST_BINS)
	@fail=0; \
	for t in $(TEST_BINS); do \
		./$$t || fail=1; \
	done; \
	exit $$fail

clean:
	rm -rf $(BUILD)
