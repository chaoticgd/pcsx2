# Disable implicit rules.
MAKEFLAGS += -r

BUILD_DIR = ../../build/system_test_ps2/

HARNESS_SOURCES := \
	harness/HarnessPS2.cpp
TEST_SOURCES := \
	testcases/ee/FPU.cpp

HARNESS_OBJECTS := $(patsubst %,$(BUILD_DIR)/%.o,$(basename $(HARNESS_SOURCES)))
TEST_OBJECTS := $(patsubst %,$(BUILD_DIR)/%.o,$(basename $(TEST_SOURCES)))

INCLUDES := -Iinclude -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include

SOURCES := $(COMMON_SOURCES) $(HARNESS_SOURCES) $(TEST_SOURCES)
OBJECTS := $(COMMON_OBJECTS) $(TEST_OBJECTS) $(TEST_OBJECTS)
DEPENDENCIES := $(patsubst %,$(BUILD_DIR)/%.d,$(basename $(SOURCES)))

LINKFLAGS = -T$(PS2SDK)/ee/startup/linkfile

all: $(BUILD_DIR)/system_test_ps2.elf

$(BUILD_DIR)/system_test_ps2.elf: $(OBJECTS)
	mkdir -p $(dir $@)
	$(EE_CXX) $(OBJECTS) -o $@ $(LINKFLAGS)

.PHONY: clean
clean:
	$(RM) $(OBJECTS) $(DEPENDENCIES)

CXX_FLAGS = -D_EE -std=c++20 -O0 -Wall -Werror -G0
DEPENDENCY_FLAGS = -MT $@ -MMD -MP -MF $(BUILD_DIR)/$*.d

$(BUILD_DIR)/%.o: %.cpp
$(BUILD_DIR)/%.o: %.cpp $(BUILD_DIR)/%.d
	mkdir -p $(dir $@)
	$(EE_CXX) -c $< -o $@ $(DEPENDENCY_FLAGS) $(CXX_FLAGS) $(INCLUDES)

$(DEPENDENCIES):

include $(wildcard $(DEPENDENCIES))

include $(PS2SDK)/samples/Makefile.pref

ifndef PS2SDK
$(error PS2SDK variable not set)
endif
