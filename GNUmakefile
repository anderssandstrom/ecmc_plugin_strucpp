include /ioc/tools/driver.makefile

MODULE = ecmc_plugin_strucpp

# Transfer module name to the ecmc plugin entry symbol.
USR_CFLAGS +=-DECMC_PLUGIN_MODULE_NAME=${MODULE}

BUILDCLASSES = Linux
ARCH_FILTER = deb10% deb12%

EXCLUDE_VERSIONS+=3 7.0.5 7.0.6 7.0.7

IGNORE_MODULES += asynMotor
IGNORE_MODULES += motorBase

USR_CXXFLAGS += -std=c++17
OPT_CXXFLAGS_YES = -O3

ecmc_VERSION = 10.0

ifndef STRUCPP
STRUCPP = $(abspath ../strucpp)
endif

USR_CXXFLAGS += -I$(STRUCPP)/src/runtime/include
LIB_SYS_LIBS += dl

BASE_DIR = .
SRC_DIR = $(BASE_DIR)/src
SCRIPTS_DIR = $(BASE_DIR)/scripts
EXAMPLES_DIR = $(BASE_DIR)/examples

SOURCES += $(SRC_DIR)/ecmcPluginStrucpp.cpp
SOURCES += $(SRC_DIR)/ecmcStrucppBridge.cpp

HEADERS += $(foreach d,${SRC_DIR}, $(wildcard $d/*.h) $(wildcard $d/*.hpp))
SCRIPTS += $(BASE_DIR)/startup.cmd
SCRIPTS += $(wildcard $(SCRIPTS_DIR)/*.cmd)
SCRIPTS += $(wildcard $(EXAMPLES_DIR)/*.cmd)
