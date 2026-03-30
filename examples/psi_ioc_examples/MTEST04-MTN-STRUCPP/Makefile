.PHONY: all install clean

IOC_NAME := MTEST04-MTN-STRUCPP
IOC_ALL_SUBS := $(IOC_NAME)_all.subs
IOC_SUBSGEN := ../../../scripts/strucpp_ioc_all_subsgen.py

all:
	$(MAKE) -C src stage
	python3 $(IOC_SUBSGEN) --output $(IOC_ALL_SUBS) --input bin/main.so.substitutions

install: all
	ioc install --clean -V --ioc $(IOC_NAME)

clean:
	$(MAKE) -C src clean

