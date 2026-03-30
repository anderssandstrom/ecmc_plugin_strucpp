.PHONY: all install clean

IOC_STARTUP_FILE := $(notdir $(firstword $(wildcard *_startup.script)))
IOC_PARAMS_FILE := $(notdir $(firstword $(wildcard *_parameters.yaml)))
IOC_NAME ?= $(patsubst %_startup.script,%,$(IOC_STARTUP_FILE))
ifeq ($(IOC_NAME),)
IOC_NAME := $(patsubst %_parameters.yaml,%,$(IOC_PARAMS_FILE))
endif
IOC_STRUCPP_SUBS := $(IOC_NAME)_strucpp.subs
IOC_SUBSGEN := ../../../scripts/strucpp_ioc_subsgen.py
IOC_QT_PANEL := qt/$(IOC_NAME)_strucpp.ui
IOC_QTGEN := ../../../scripts/strucpp_ioc_qtgen.py
GENERATE_QT ?= 1

all:
	$(MAKE) -C src stage
	python3 $(IOC_SUBSGEN) --ioc-name $(IOC_NAME) --output $(IOC_STRUCPP_SUBS) --input bin/main.so.substitutions
	if [ "$(GENERATE_QT)" = "1" ]; then python3 $(IOC_QTGEN) --input $(IOC_STRUCPP_SUBS) --output $(IOC_QT_PANEL); fi

install: all
	ioc install --clean -V --ioc $(IOC_NAME)

clean:
	$(MAKE) -C src clean
	$(RM) $(IOC_STRUCPP_SUBS)
	$(RM) $(IOC_QT_PANEL)
