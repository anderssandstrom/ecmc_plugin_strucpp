.PHONY: all install clean

IOC_NAME := $(basename $(notdir $(firstword $(wildcard *_parameters.yaml))))
IOC_ALL_SUBS := $(IOC_NAME)_all.subs
IOC_SUBSGEN := ../../../../scripts/strucpp_ioc_all_subsgen.py

all:
	$(MAKE) -C src stage
	python3 $(IOC_SUBSGEN) --output $(IOC_ALL_SUBS) --input src/bin/main.so.substitutions

install: all
	ioc install --source .

clean:
	$(MAKE) -C src clean
