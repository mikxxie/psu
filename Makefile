# PSU Makefile
# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -I. -pthread
LDFLAGS = -lm -lpthread -lcurl

# Directories
SRC_DIR = .
LIB_DIR = libs
BUILD_DIR = build
BIN_DIR = .

# Source files
MAIN_SRC = $(SRC_DIR)/main.c
LIB_SRCS = $(wildcard $(LIB_DIR)/*.c)
SRCS = $(MAIN_SRC) $(LIB_SRCS)

# Object files
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(MAIN_SRC)) \
       $(patsubst $(LIB_DIR)/%.c,$(BUILD_DIR)/%.o,$(LIB_SRCS))

# Target executable
TARGET = $(BIN_DIR)/psu

# Phony targets
.PHONY: all clean distclean install uninstall test

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

# Compile main.c
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile libs/*.c
$(BUILD_DIR)/%.o: $(LIB_DIR)/%.c $(LIB_DIR)/%.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET)
	@echo "Cleaned build files"

# Distclean (remove everything)
distclean: clean
	rm -f $(TARGET)
	@echo "Distribution cleaned"

# Install (copy to /usr/local/bin)
install: $(TARGET)
	@sudo cp $(TARGET) /usr/local/bin/
	@echo "Installed to /usr/local/bin/psu"

# Uninstall
uninstall:
	@sudo rm -f /usr/local/bin/psu
	@echo "Uninstalled from /usr/local/bin/psu"

# Test with example files
test: $(TARGET)
	@echo "Running tests..."
	@if [ -f examples/system.psu ]; then \
		./$(TARGET) examples/system.psu; \
	fi
	@if [ -f examples/test.psu ]; then \
		./$(TARGET) examples/test.psu; \
	fi

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: clean all

# Help
help:
	@echo "PSU Makefile targets:"
	@echo "  all       - Build the psu executable (default)"
	@echo "  clean     - Remove object files"
	@echo "  distclean - Remove all build files and executable"
	@echo "  install   - Install to /usr/local/bin (requires sudo)"
	@echo "  uninstall - Remove from /usr/local/bin (requires sudo)"
	@echo "  test      - Run tests with example files"
	@echo "  debug     - Build with debug symbols"
	@echo "  help      - Show this help message"
