BUILD_DIR ?= build

STRUCPP ?= $(abspath ../../../../strucpp)
ECMC_PLUGIN_STRUCPP ?= $(abspath ../../..)
PODMAN ?= podman
NODE_IMAGE ?= docker.io/library/node:20
ECMC_MOTION_LIB_DIR ?= $(ECMC_PLUGIN_STRUCPP)/libs
ECMC_MOTION_STLIB := $(ECMC_MOTION_LIB_DIR)/ecmc-motion.stlib

LOGIC_MODULES := machine_logic el7041_velocity_logic motion_actpos_mirror_logic motion_velocity_direct_logic mc_power_move_abs_logic mc_power_move_absolute_lib_logic mc_power_move_velocity_lib_logic mc_power_move_relative_lib_logic
PYTHON ?= python3
MAPGEN := $(ECMC_PLUGIN_STRUCPP)/scripts/strucpp_mapgen.py
EXPORTGEN := $(ECMC_PLUGIN_STRUCPP)/scripts/strucpp_epics_exportgen.py
SUBSTGEN := $(ECMC_PLUGIN_STRUCPP)/scripts/strucpp_epics_substgen.py
MAPGEN_DEFINES ?= AXIS_INDEX=1
MAPGEN_DEFINE_ARGS := $(foreach def,$(MAPGEN_DEFINES),--define $(def))

CXX ?= c++
CXXFLAGS += -std=c++17 -fPIC -Wall -Wextra
ECMC ?= $(abspath ../../../../ecmc)
CPPFLAGS += -I$(STRUCPP)/src/runtime/include
CPPFLAGS += -I$(ECMC_PLUGIN_STRUCPP)/src
CPPFLAGS += -I$(ECMC)/devEcmcSup/motion
CPPFLAGS += -Isrc
CPPFLAGS += -Isrc/generated

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
LDFLAGS += -dynamiclib -undefined dynamic_lookup
LIBEXT := dylib
else
LDFLAGS += -shared
LIBEXT := so
endif

MACHINE_SOURCES := \
	src/generated/machine.cpp \
	src/machine_logic.cpp

EL7041_VELOCITY_SOURCES := \
	src/generated/el7041_velocity.cpp \
	src/el7041_velocity_logic.cpp

MOTION_ACTPOS_MIRROR_SOURCES := \
	src/generated/motion_actpos_mirror.cpp \
	src/motion_actpos_mirror_logic.cpp

MOTION_VELOCITY_DIRECT_SOURCES := \
	src/generated/motion_velocity_direct.cpp \
	src/motion_velocity_direct_logic.cpp

MC_POWER_MOVE_ABS_SOURCES := \
	src/mc_power_move_abs_logic.cpp

MC_POWER_MOVE_ABSOLUTE_LIB_SOURCES := \
	src/generated/mc_power_move_absolute_lib.cpp \
	src/mc_power_move_absolute_lib_logic.cpp

MC_POWER_MOVE_VELOCITY_LIB_SOURCES := \
	src/generated/mc_power_move_velocity_lib.cpp \
	src/mc_power_move_velocity_lib_logic.cpp

MC_POWER_MOVE_RELATIVE_LIB_SOURCES := \
	src/generated/mc_power_move_relative_lib.cpp \
	src/mc_power_move_relative_lib_logic.cpp

MACHINE_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(MACHINE_SOURCES))
EL7041_VELOCITY_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(EL7041_VELOCITY_SOURCES))
MOTION_ACTPOS_MIRROR_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(MOTION_ACTPOS_MIRROR_SOURCES))
MOTION_VELOCITY_DIRECT_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(MOTION_VELOCITY_DIRECT_SOURCES))
MC_POWER_MOVE_ABS_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(MC_POWER_MOVE_ABS_SOURCES))
MC_POWER_MOVE_ABSOLUTE_LIB_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(MC_POWER_MOVE_ABSOLUTE_LIB_SOURCES))
MC_POWER_MOVE_VELOCITY_LIB_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(MC_POWER_MOVE_VELOCITY_LIB_SOURCES))
MC_POWER_MOVE_RELATIVE_LIB_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(MC_POWER_MOVE_RELATIVE_LIB_SOURCES))

MACHINE_TARGET := $(BUILD_DIR)/machine_logic.$(LIBEXT)
EL7041_VELOCITY_TARGET := $(BUILD_DIR)/el7041_velocity_logic.$(LIBEXT)
MOTION_ACTPOS_MIRROR_TARGET := $(BUILD_DIR)/motion_actpos_mirror_logic.$(LIBEXT)
MOTION_VELOCITY_DIRECT_TARGET := $(BUILD_DIR)/motion_velocity_direct_logic.$(LIBEXT)
MC_POWER_MOVE_ABS_TARGET := $(BUILD_DIR)/mc_power_move_abs_logic.$(LIBEXT)
MC_POWER_MOVE_ABSOLUTE_LIB_TARGET := $(BUILD_DIR)/mc_power_move_absolute_lib_logic.$(LIBEXT)
MC_POWER_MOVE_VELOCITY_LIB_TARGET := $(BUILD_DIR)/mc_power_move_velocity_lib_logic.$(LIBEXT)
MC_POWER_MOVE_RELATIVE_LIB_TARGET := $(BUILD_DIR)/mc_power_move_relative_lib_logic.$(LIBEXT)
MACHINE_MAP := $(MACHINE_TARGET).map
MACHINE_EXPORTS := src/generated/machine_epics_exports.hpp
MACHINE_SUBSTITUTIONS := $(MACHINE_TARGET).substitutions
EL7041_VELOCITY_MAP := $(EL7041_VELOCITY_TARGET).map
MOTION_ACTPOS_MIRROR_MAP := $(MOTION_ACTPOS_MIRROR_TARGET).map
MOTION_VELOCITY_DIRECT_MAP := $(MOTION_VELOCITY_DIRECT_TARGET).map
TARGETS := $(MACHINE_TARGET) $(EL7041_VELOCITY_TARGET) $(MOTION_ACTPOS_MIRROR_TARGET) $(MC_POWER_MOVE_ABS_TARGET) $(MC_POWER_MOVE_ABSOLUTE_LIB_TARGET) $(MC_POWER_MOVE_VELOCITY_LIB_TARGET) $(MC_POWER_MOVE_RELATIVE_LIB_TARGET) $(MACHINE_MAP) $(MACHINE_SUBSTITUTIONS) $(EL7041_VELOCITY_MAP) $(MOTION_ACTPOS_MIRROR_MAP)
TARGETS += $(MOTION_VELOCITY_DIRECT_TARGET) $(MOTION_VELOCITY_DIRECT_MAP)

.PHONY: all clean regen regen-container maps check-motion-lib

all: $(TARGETS)

$(MACHINE_TARGET): $(MACHINE_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(MACHINE_OBJECTS) $(LDFLAGS) -o $@

$(EL7041_VELOCITY_TARGET): $(EL7041_VELOCITY_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(EL7041_VELOCITY_OBJECTS) $(LDFLAGS) -o $@

$(MOTION_ACTPOS_MIRROR_TARGET): $(MOTION_ACTPOS_MIRROR_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(MOTION_ACTPOS_MIRROR_OBJECTS) $(LDFLAGS) -o $@

$(MOTION_VELOCITY_DIRECT_TARGET): $(MOTION_VELOCITY_DIRECT_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(MOTION_VELOCITY_DIRECT_OBJECTS) $(LDFLAGS) -o $@

$(MC_POWER_MOVE_ABS_TARGET): $(MC_POWER_MOVE_ABS_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(MC_POWER_MOVE_ABS_OBJECTS) $(LDFLAGS) -o $@

$(MC_POWER_MOVE_ABSOLUTE_LIB_TARGET): $(MC_POWER_MOVE_ABSOLUTE_LIB_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(MC_POWER_MOVE_ABSOLUTE_LIB_OBJECTS) $(LDFLAGS) -o $@

$(MC_POWER_MOVE_VELOCITY_LIB_TARGET): $(MC_POWER_MOVE_VELOCITY_LIB_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(MC_POWER_MOVE_VELOCITY_LIB_OBJECTS) $(LDFLAGS) -o $@

$(MC_POWER_MOVE_RELATIVE_LIB_TARGET): $(MC_POWER_MOVE_RELATIVE_LIB_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(MC_POWER_MOVE_RELATIVE_LIB_OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/machine_logic.o: $(MACHINE_EXPORTS)

$(MACHINE_EXPORTS): src/generated/machine.hpp st/machine.st $(EXPORTGEN)
	$(PYTHON) $(EXPORTGEN) --st-source st/machine.st --header src/generated/machine.hpp --header-include generated/machine.hpp --output $@

$(MACHINE_SUBSTITUTIONS): st/machine.st $(SUBSTGEN)
	@mkdir -p $(dir $@)
	$(PYTHON) $(SUBSTGEN) --st-source st/machine.st --output $@

$(MACHINE_MAP): src/generated/machine.hpp st/machine.st $(MAPGEN)
	@mkdir -p $(dir $@)
	$(PYTHON) $(MAPGEN) --header src/generated/machine.hpp --st-source st/machine.st --output $@

$(EL7041_VELOCITY_MAP): src/generated/el7041_velocity.hpp st/el7041_velocity.st $(MAPGEN)
	@mkdir -p $(dir $@)
	$(PYTHON) $(MAPGEN) --header src/generated/el7041_velocity.hpp --st-source st/el7041_velocity.st --output $@

$(MOTION_ACTPOS_MIRROR_MAP): src/generated/motion_actpos_mirror.hpp st/motion_actpos_mirror.st $(MAPGEN)
	@mkdir -p $(dir $@)
	$(PYTHON) $(MAPGEN) $(MAPGEN_DEFINE_ARGS) --header src/generated/motion_actpos_mirror.hpp --st-source st/motion_actpos_mirror.st --output $@

$(MOTION_VELOCITY_DIRECT_MAP): src/generated/motion_velocity_direct.hpp st/motion_velocity_direct.st $(MAPGEN)
	@mkdir -p $(dir $@)
	$(PYTHON) $(MAPGEN) $(MAPGEN_DEFINE_ARGS) --header src/generated/motion_velocity_direct.hpp --st-source st/motion_velocity_direct.st --output $@

check-motion-lib:
	@test -f $(ECMC_MOTION_STLIB) || (echo "Missing motion library: $(ECMC_MOTION_STLIB)" >&2; exit 1)

regen: check-motion-lib
	strucpp st/machine.st -o src/generated/machine.cpp
	strucpp st/el7041_velocity.st -o src/generated/el7041_velocity.cpp
	strucpp st/motion_actpos_mirror.st -o src/generated/motion_actpos_mirror.cpp
	strucpp st/motion_velocity_direct.st -o src/generated/motion_velocity_direct.cpp
	strucpp st/mc_power_move_absolute_lib.st -o src/generated/mc_power_move_absolute_lib.cpp -L $(ECMC_MOTION_LIB_DIR)
	strucpp st/mc_power_move_velocity_lib.st -o src/generated/mc_power_move_velocity_lib.cpp -L $(ECMC_MOTION_LIB_DIR)
	strucpp st/mc_power_move_relative_lib.st -o src/generated/mc_power_move_relative_lib.cpp -L $(ECMC_MOTION_LIB_DIR)

regen-container: check-motion-lib
	$(PODMAN) run --rm \
		-v $(STRUCPP):/src/strucpp:ro \
		-v $(CURDIR):/src/app \
		-v $(ECMC_PLUGIN_STRUCPP):/src/plugin:ro \
		$(NODE_IMAGE) \
		bash -lc "set -euo pipefail; cp -R /src/strucpp /tmp/strucpp; cd /tmp/strucpp; npm ci --ignore-scripts; npm run build; node dist/cli.js /src/app/st/machine.st -o /src/app/src/generated/machine.cpp; node dist/cli.js /src/app/st/el7041_velocity.st -o /src/app/src/generated/el7041_velocity.cpp; node dist/cli.js /src/app/st/motion_actpos_mirror.st -o /src/app/src/generated/motion_actpos_mirror.cpp; node dist/cli.js /src/app/st/motion_velocity_direct.st -o /src/app/src/generated/motion_velocity_direct.cpp; node dist/cli.js /src/app/st/mc_power_move_absolute_lib.st -o /src/app/src/generated/mc_power_move_absolute_lib.cpp -L /src/plugin/libs; node dist/cli.js /src/app/st/mc_power_move_velocity_lib.st -o /src/app/src/generated/mc_power_move_velocity_lib.cpp -L /src/plugin/libs; node dist/cli.js /src/app/st/mc_power_move_relative_lib.st -o /src/app/src/generated/mc_power_move_relative_lib.cpp -L /src/plugin/libs"

maps: $(MACHINE_MAP) $(EL7041_VELOCITY_MAP) $(MOTION_ACTPOS_MIRROR_MAP) $(MOTION_VELOCITY_DIRECT_MAP)

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(MACHINE_EXPORTS)
