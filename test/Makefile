# ============================================================================
# PlayMode — native (host) test build for the General-Midi-Boop v2 layer
# ============================================================================
#
# The GMB layer is written against plain C++ and the PlayMode configuration
# structs, so it compiles and runs on a workstation or in CI with no ESP32
# toolchain. `make -C test` builds and runs everything.
#

CXX      ?= g++
CXXFLAGS := -std=c++11 -O1 -g -Wall -Wextra -Wno-unused-parameter -Werror

SRC_DIR  := ../play-mode
INCLUDES := -I. -Iarduino_shim -I$(SRC_DIR)

# Firmware translation units under test (host-compilable by construction).
FW_SRCS := \
	$(SRC_DIR)/gmb_capabilities.cpp \
	$(SRC_DIR)/gmb_descriptor.cpp \
	$(SRC_DIR)/gmb_gm_program.cpp \
	$(SRC_DIR)/gmb_instance_id.cpp \
	$(SRC_DIR)/gmb_revision.cpp \
	$(SRC_DIR)/gmb_sysex.cpp \
	$(SRC_DIR)/gmb_sysex_service.cpp \
	$(SRC_DIR)/gmb_runtime.cpp \
	$(SRC_DIR)/midi_parser.cpp

TEST_SRCS := $(wildcard test_*.cpp)

BUILD_DIR := build
OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(notdir $(TEST_SRCS))) \
        $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/fw_%.o,$(FW_SRCS))

BIN := $(BUILD_DIR)/gmb_tests

.PHONY: all test clean
all: test

test: $(BIN)
	@./$(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD_DIR)/fw_%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

clean:
	@rm -rf $(BUILD_DIR)
