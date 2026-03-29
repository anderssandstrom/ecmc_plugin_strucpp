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

ecmc_VERSION = strucpp

ifndef STRUCPP
STRUCPP = $(abspath ../../STruCpp)
endif

USR_CXXFLAGS += -I$(STRUCPP)/src/runtime/include
LIB_SYS_LIBS += dl

BASE_DIR = .
SRC_DIR = $(BASE_DIR)/src
SCRIPTS_DIR = $(BASE_DIR)/scripts
EXAMPLES_DIR = $(BASE_DIR)/examples
DB_DIR = $(BASE_DIR)/db
LIB_DIR = $(BASE_DIR)/lib
LIBS_DIR = $(BASE_DIR)/libs
TEMPLATES_DIR = $(BASE_DIR)/templates

SOURCES += $(SRC_DIR)/ecmcPluginStrucpp.cpp
SOURCES += $(SRC_DIR)/ecmcStrucppBridge.cpp
TEMPLATES += $(wildcard $(DB_DIR)/*.template)

HEADERS += $(foreach d,${SRC_DIR}, $(wildcard $d/*.h) $(wildcard $d/*.hpp))
SCRIPTS += $(BASE_DIR)/startup.cmd
SCRIPTS += $(wildcard $(SCRIPTS_DIR)/*.cmd)
SCRIPTS += $(wildcard $(SCRIPTS_DIR)/*.py)
SCRIPTS += $(wildcard $(SCRIPTS_DIR)/*.sh)
SCRIPTS += $(wildcard $(EXAMPLES_DIR)/*.cmd)
SCRIPTS += $(wildcard $(DB_DIR)/*.template)
SCRIPTS += $(wildcard $(LIB_DIR)/*.st)
SCRIPTS += $(wildcard $(LIBS_DIR)/*.stlib)
SCRIPTS += $(wildcard $(TEMPLATES_DIR)/*)
