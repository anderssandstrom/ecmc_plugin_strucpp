PROGRAM ?= machine
LOGIC_NAME ?= $(PROGRAM)_logic
ST_SOURCE ?= $(PROGRAM).st
ST_SOURCES ?= $(ST_SOURCE)
EXTRA_CPP_SOURCES ?=
PROJECT_BIN_DIR ?= ../bin
GEN_DIR ?= generated

STRUCPP ?= ../../../strucpp
STRUCPP_CLI ?= strucpp
ECMC_PLUGIN_STRUCPP ?= ../../../ecmc_plugin_strucpp
PYTHON ?= python3
EPICS_VERSION ?= 3.14.12
OS_CLASS ?= RHEL7
CPU_ARCH ?= x86_64

MAPGEN := $(ECMC_PLUGIN_STRUCPP)/scripts/strucpp_mapgen.py
EXPORTGEN := $(ECMC_PLUGIN_STRUCPP)/scripts/strucpp_epics_exportgen.py
SUBSTGEN := $(ECMC_PLUGIN_STRUCPP)/scripts/strucpp_epics_substgen.py
WRAPPERGEN := $(ECMC_PLUGIN_STRUCPP)/scripts/strucpp_logic_wrappergen.py
BUNDLEGEN := $(ECMC_PLUGIN_STRUCPP)/scripts/strucpp_bundle_st.py

CXX ?= c++
CXXFLAGS += -std=c++17 -fPIC -Wall -Wextra
CPPFLAGS += -I$(STRUCPP)/src/runtime/include
CPPFLAGS += -I$(ECMC_PLUGIN_STRUCPP)/src
CPPFLAGS += -I.
CPPFLAGS += -I$(GEN_DIR)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
LDFLAGS += -dynamiclib -undefined dynamic_lookup
LIBEXT := dylib
else
LDFLAGS += -shared
LIBEXT := so
endif

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

STAGED_LOGIC_LIB := $(PROJECT_BIN_DIR)/$(LOGIC_NAME).$(LIBEXT)
STAGED_MAP_FILE := $(STAGED_LOGIC_LIB).map
STAGED_SUBST_FILE := $(STAGED_LOGIC_LIB).substitutions

EXTRA_OBJS := $(patsubst %.cpp,$(ODIR)/%.o,$(EXTRA_CPP_SOURCES))
PROGRAM_OBJ := $(ODIR)/$(PROGRAM)_program.o
WRAPPER_OBJ := $(ODIR)/$(LOGIC_NAME)_wrapper.o
OBJS := $(PROGRAM_OBJ) $(WRAPPER_OBJ) $(EXTRA_OBJS)

.PHONY: all clean regen maps stage

all: $(LOGIC_LIB) $(MAP_FILE) $(SUBST_FILE)

stage: all $(STAGED_LOGIC_LIB) $(STAGED_MAP_FILE) $(STAGED_SUBST_FILE)

regen: $(GEN_CPP)

$(ST_BUNDLE): $(ST_SOURCES) $(BUNDLEGEN)
	mkdir -p $(GEN_DIR)
	$(PYTHON) $(BUNDLEGEN) --output $@ $(ST_SOURCES)

$(GEN_CPP): $(ST_BUNDLE)
	mkdir -p $(GEN_DIR)
	$(STRUCPP_CLI) $< -o $@

$(GEN_HPP): $(GEN_CPP)

$(EXPORTS_HPP): $(GEN_HPP) $(ST_BUNDLE) $(EXPORTGEN)
	$(PYTHON) $(EXPORTGEN) --st-source $(ST_BUNDLE) --header $(GEN_HPP) --header-include $(GEN_DIR)/$(PROGRAM).hpp --output $@

$(GENERATED_WRAPPER_CPP): $(ST_BUNDLE) $(WRAPPERGEN)
	mkdir -p $(GEN_DIR)
	$(PYTHON) $(WRAPPERGEN) --st-source $(ST_BUNDLE) --logic-name $(LOGIC_NAME) --header-include $(GEN_DIR)/$(PROGRAM).hpp --exports-include $(GEN_DIR)/$(PROGRAM)_epics_exports.hpp --output $@

$(MAP_FILE): $(GEN_HPP) $(ST_BUNDLE) $(MAPGEN)
	mkdir -p $(ODIR)
	$(PYTHON) $(MAPGEN) --header $(GEN_HPP) --st-source $(ST_BUNDLE) --output $@

$(SUBST_FILE): $(ST_BUNDLE) $(SUBSTGEN)
	mkdir -p $(ODIR)
	$(PYTHON) $(SUBSTGEN) --st-source $(ST_BUNDLE) --output $@

$(PROGRAM_OBJ): $(GEN_CPP)
	mkdir -p $(ODIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(WRAPPER_OBJ): $(WRAPPER_CPP) $(EXPORTS_HPP)
	mkdir -p $(ODIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(ODIR)/%.o: %.cpp
	mkdir -p $(dir $@)
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

maps: $(MAP_FILE) $(SUBST_FILE)

clean:
	rm -rf $(ODIR) $(GEN_DIR) $(PROJECT_BIN_DIR)
