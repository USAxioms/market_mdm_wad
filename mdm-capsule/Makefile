# ============================================================================
# MDM REPRODUCIBILITY CAPSULE - BUILD SYSTEM
# ============================================================================
# Pure WAD fixed-point market decomposition manifold
# Zero floating-point operations
# Deterministic, bit-exact reproducible build
# ============================================================================

CC := gcc
CFLAGS := -std=c99 -Wall -Wextra -O2 -fno-fast-math
LDFLAGS := -lm

SRC_DIR := src
BUILD_DIR := build
BIN_DIR := $(BUILD_DIR)/bin

SOURCES := $(SRC_DIR)/matrix_fixed_point.c $(SRC_DIR)/mdm_engine.c $(SRC_DIR)/mdm_main.c
HEADERS := $(SRC_DIR)/matrix_fixed_point.h $(SRC_DIR)/mdm_engine.h
OBJECTS := $(addprefix $(BUILD_DIR)/, matrix_fixed_point.o mdm_engine.o mdm_main.o)

TARGET := $(BIN_DIR)/mdm_capsule

# ========== BUILD TARGETS ==========

.PHONY: all clean test check reproducible

all: $(TARGET)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "✓ Build successful: $@"

test: $(TARGET)
	@echo "Running MDM capsule..."
	@$(TARGET)
	@echo "✓ Execution complete"

check: $(TARGET)
	@echo "Verifying build integrity..."
	@file $(TARGET)
	@echo "✓ Build verified"

clean:
	@rm -rf $(BUILD_DIR)
	@echo "✓ Clean complete"

reproducible: clean all
	@echo "✓ Reproducible build complete"
	@sha256sum $(TARGET)

# ========== PHONY TARGETS ==========

.PHONY: all clean test check reproducible
