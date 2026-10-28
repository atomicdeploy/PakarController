CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -std=c11 -g
LDFLAGS = 

# Directories
SRC_DIR = src
INC_DIR = include
EXAMPLE_DIR = examples
TEST_DIR = tests
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# Library
LIB = $(BUILD_DIR)/libportableshell.a

# Examples
EXAMPLES = $(BUILD_DIR)/linux_example $(BUILD_DIR)/telnet_server

# Tests
TESTS = $(BUILD_DIR)/test_shell

.PHONY: all clean examples test

all: $(LIB) examples

# Build library
$(LIB): $(OBJS) | $(BUILD_DIR)
	ar rcs $@ $(OBJS)
	@echo "Library created: $@"

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build examples
examples: $(EXAMPLES)

$(BUILD_DIR)/linux_example: $(EXAMPLE_DIR)/linux_example.c $(LIB)
	$(CC) $(CFLAGS) $< -L$(BUILD_DIR) -lportableshell $(LDFLAGS) -o $@
	@echo "Built: $@"

$(BUILD_DIR)/telnet_server: $(EXAMPLE_DIR)/telnet_server.c $(LIB)
	$(CC) $(CFLAGS) $< -L$(BUILD_DIR) -lportableshell $(LDFLAGS) -o $@
	@echo "Built: $@"

# Build tests
test: $(TESTS)
	@echo "Running tests..."
	@$(BUILD_DIR)/test_shell

$(BUILD_DIR)/test_shell: $(TEST_DIR)/test_shell.c $(LIB)
	$(CC) $(CFLAGS) $< -L$(BUILD_DIR) -lportableshell $(LDFLAGS) -o $@
	@echo "Built: $@"

# Create directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Install (optional)
install: $(LIB)
	mkdir -p /usr/local/lib
	mkdir -p /usr/local/include
	cp $(LIB) /usr/local/lib/
	cp $(INC_DIR)/*.h /usr/local/include/

# Uninstall
uninstall:
	rm -f /usr/local/lib/libportableshell.a
	rm -f /usr/local/include/portable_shell.h
	rm -f /usr/local/include/ps_io.h
