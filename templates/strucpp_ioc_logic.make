PROGRAM ?= main
LOGIC_NAME ?= $(PROGRAM)
ST_SOURCE ?= $(PROGRAM).st
INCLUDE_DEBUG_ST ?= 1
INCLUDE_CONTROL_ST ?= 1
INCLUDE_UTILS_ST ?= 1
INCLUDE_MOTION_ST ?= 0
COMMON_ST_SOURCES :=
ifeq ($(INCLUDE_CONTROL_ST),1)
COMMON_ST_SOURCES += $(ECMC_PLUGIN_STRUCPP)/lib/ecmc_control.st
endif
ifeq ($(INCLUDE_UTILS_ST),1)
COMMON_ST_SOURCES += $(ECMC_PLUGIN_STRUCPP)/lib/ecmc_utils.st
endif
ifeq ($(INCLUDE_DEBUG_ST),1)
COMMON_ST_SOURCES += $(ECMC_PLUGIN_STRUCPP)/lib/ecmc_debug.st
endif
ST_SOURCES ?= $(COMMON_ST_SOURCES) $(ST_SOURCE)
ANNOTATION_DEFINES ?=
EXTRA_CPP_SOURCES ?=
PROJECT_BIN_DIR ?= ../bin
GEN_DIR ?= generated

STRUCPP ?= ../../../strucpp
ECMC_PLUGIN_STRUCPP ?= ../../../ecmc_plugin_strucpp
ECMC ?= $(abspath $(ECMC_PLUGIN_STRUCPP)/../ECMC/ecmc)
PYTHON ?= python3
EPICS_VERSION ?= 7.0.10
OS_CLASS ?= deb12
CPU_ARCH ?= x86_64

DEFAULT_STRUCPP_CLI := $(abspath $(ECMC_PLUGIN_STRUCPP)/../strucpp_bin/strucpp)
ifeq ($(origin STRUCPP_CLI), undefined)
  ifneq ($(wildcard $(DEFAULT_STRUCPP_CLI)),)
    STRUCPP_CLI := $(DEFAULT_STRUCPP_CLI)
  else
    STRUCPP_CLI := strucpp
  endif
endif
STRUCPP_LIB_PATHS :=
ifeq ($(INCLUDE_MOTION_ST),1)
STRUCPP_LIB_PATHS += $(ECMC_PLUGIN_STRUCPP)/libs
endif
STRUCPP_LIB_ARGS := $(foreach dir,$(STRUCPP_LIB_PATHS),-L $(dir))

MAPGEN := $(ECMC_PLUGIN_STRUCPP)/scripts/strucpp_mapgen.py
EXPORTGEN := $(ECMC_PLUGIN_STRUCPP)/scripts/strucpp_epics_exportgen.py
SUBSTGEN := $(ECMC_PLUGIN_STRUCPP)/scripts/strucpp_epics_substgen.py
WRAPPERGEN := $(ECMC_PLUGIN_STRUCPP)/scripts/strucpp_logic_wrappergen.py
BUNDLEGEN := $(ECMC_PLUGIN_STRUCPP)/scripts/strucpp_bundle_st.py
REPORTGEN := $(ECMC_PLUGIN_STRUCPP)/scripts/strucpp_logic_report.py
ANNOTATION_DEFINE_ARGS := $(foreach def,$(ANNOTATION_DEFINES),--define $(def))
HELPER_CPP_SOURCES := $(ECMC_PLUGIN_STRUCPP)/src/ecmcStrucppDebug.cpp \
	$(ECMC_PLUGIN_STRUCPP)/src/ecmcStrucppUtil.cpp

CXX ?= c++
CXXFLAGS += -std=c++17 -fPIC -Wall -Wextra
CPPFLAGS += -I$(STRUCPP)/src/runtime/include
CPPFLAGS += -I$(ECMC_PLUGIN_STRUCPP)/src
ifeq ($(INCLUDE_MOTION_ST),1)
CPPFLAGS += -I$(ECMC)/devEcmcSup/motion
endif
CPPFLAGS += -I.
CPPFLAGS += -I$(GEN_DIR)

LDFLAGS += -shared
LIBEXT := so

ODIR := O.$(EPICS_VERSION)_$(OS_CLASS)-$(CPU_ARCH)

ST_BUNDLE := $(GEN_DIR)/$(PROGRAM)_bundle.st
GEN_CPP := $(GEN_DIR)/$(PROGRAM).cpp
GEN_HPP := $(GEN_DIR)/$(PROGRAM).hpp
EXPORTS_HPP := $(GEN_DIR)/$(PROGRAM)_epics_exports.hpp
GENERATED_WRAPPER_CPP := $(GEN_DIR)/$(LOGIC_NAME)_wrapper.cpp
WRAPPER_CPP ?= $(GENERATED_WRAPPER_CPP)

LOGIC_LIB := $(ODIR)/$(LOGIC_NAME).$(LIBEXT)
MAP_FILE := $(LOGIC_LIB).map
SUBST_FILE := $(LOGIC_LIB).substitutions
SUMMARY_FILE := $(LOGIC_LIB).summary.txt

STAGED_LOGIC_LIB := $(PROJECT_BIN_DIR)/$(LOGIC_NAME).so
STAGED_MAP_FILE := $(STAGED_LOGIC_LIB).map
STAGED_SUBST_FILE := $(STAGED_LOGIC_LIB).substitutions
STAGED_SUMMARY_FILE := $(STAGED_LOGIC_LIB).summary.txt

HELPER_OBJS := $(patsubst $(ECMC_PLUGIN_STRUCPP)/src/%.cpp,$(ODIR)/%.o,$(HELPER_CPP_SOURCES))
EXTRA_OBJS := $(patsubst %.cpp,$(ODIR)/%.o,$(EXTRA_CPP_SOURCES))
PROGRAM_OBJ := $(ODIR)/$(PROGRAM)_program.o
WRAPPER_OBJ := $(ODIR)/$(LOGIC_NAME)_wrapper.o
OBJS := $(PROGRAM_OBJ) $(WRAPPER_OBJ) $(EXTRA_OBJS) $(HELPER_OBJS)

.PHONY: all clean regen maps stage validate

all: $(LOGIC_LIB) $(MAP_FILE) $(SUBST_FILE) $(SUMMARY_FILE)

stage: all $(STAGED_LOGIC_LIB) $(STAGED_MAP_FILE) $(STAGED_SUBST_FILE) $(STAGED_SUMMARY_FILE)

regen: $(GEN_CPP)

$(ST_BUNDLE): $(ST_SOURCES) $(BUNDLEGEN)
	mkdir -p $(GEN_DIR)
	$(PYTHON) $(BUNDLEGEN) $(ANNOTATION_DEFINE_ARGS) --output $@ $(ST_SOURCES)

$(GEN_CPP): $(ST_BUNDLE)
	mkdir -p $(GEN_DIR)
	$(STRUCPP_CLI) $(STRUCPP_LIB_ARGS) $< -o $@

$(GEN_HPP): $(GEN_CPP)

$(EXPORTS_HPP): $(GEN_HPP) $(ST_BUNDLE) $(EXPORTGEN)
	$(PYTHON) $(EXPORTGEN) --st-source $(ST_BUNDLE) --header $(GEN_HPP) --header-include $(GEN_DIR)/$(PROGRAM).hpp --output $@

$(GENERATED_WRAPPER_CPP): $(ST_BUNDLE) $(WRAPPERGEN)
	mkdir -p $(GEN_DIR)
	$(PYTHON) $(WRAPPERGEN) --st-source $(ST_BUNDLE) --logic-name $(LOGIC_NAME) --header-include $(GEN_DIR)/$(PROGRAM).hpp --exports-include $(GEN_DIR)/$(PROGRAM)_epics_exports.hpp --output $@

$(MAP_FILE): $(GEN_HPP) $(ST_BUNDLE) $(MAPGEN)
	mkdir -p $(ODIR)
	$(PYTHON) $(MAPGEN) $(ANNOTATION_DEFINE_ARGS) --header $(GEN_HPP) --st-source $(ST_BUNDLE) --output $@

$(SUBST_FILE): $(ST_BUNDLE) $(SUBSTGEN)
	mkdir -p $(ODIR)
	$(PYTHON) $(SUBSTGEN) --st-source $(ST_BUNDLE) --output $@

$(SUMMARY_FILE): $(GEN_HPP) $(MAP_FILE) $(SUBST_FILE) $(ST_BUNDLE) $(REPORTGEN)
	mkdir -p $(ODIR)
	$(PYTHON) $(REPORTGEN) $(ANNOTATION_DEFINE_ARGS) --st-source $(ST_BUNDLE) --header $(GEN_HPP) --map $(MAP_FILE) --substitutions $(SUBST_FILE) --logic-lib $(LOGIC_LIB) --output $@

$(PROGRAM_OBJ): $(GEN_CPP)
	mkdir -p $(ODIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(WRAPPER_OBJ): $(WRAPPER_CPP) $(EXPORTS_HPP)
	mkdir -p $(ODIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(ODIR)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(ODIR)/%.o: $(ECMC_PLUGIN_STRUCPP)/src/%.cpp
	mkdir -p $(ODIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(LOGIC_LIB): $(OBJS)
	mkdir -p $(ODIR)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

$(STAGED_LOGIC_LIB): $(LOGIC_LIB)
	mkdir -p $(PROJECT_BIN_DIR)
	cp $< $@

$(STAGED_MAP_FILE): $(MAP_FILE)
	mkdir -p $(PROJECT_BIN_DIR)
	cp $< $@

$(STAGED_SUBST_FILE): $(SUBST_FILE)
	mkdir -p $(PROJECT_BIN_DIR)
	cp $< $@

$(STAGED_SUMMARY_FILE): $(SUMMARY_FILE)
	mkdir -p $(PROJECT_BIN_DIR)
	cp $< $@

maps: $(MAP_FILE) $(SUBST_FILE)

validate: $(GEN_HPP) $(MAP_FILE) $(SUBST_FILE) $(ST_BUNDLE) $(REPORTGEN)
	$(PYTHON) $(REPORTGEN) $(ANNOTATION_DEFINE_ARGS) --st-source $(ST_BUNDLE) --header $(GEN_HPP) --map $(MAP_FILE) --substitutions $(SUBST_FILE) --logic-lib $(LOGIC_LIB) --validate-only

clean:
	rm -rf $(ODIR) $(GEN_DIR) $(PROJECT_BIN_DIR)
