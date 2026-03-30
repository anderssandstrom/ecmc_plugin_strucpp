.PHONY: all install clean

IOC_NAME := MC-MOTION-STRUCPP-IOC
IOC_STRUCPP_SUBS := $(IOC_NAME)_strucpp.subs
IOC_SUBSGEN := ../../../scripts/strucpp_ioc_subsgen.py

all:
	$(MAKE) -C src stage
	python3 $(IOC_SUBSGEN) --output $(IOC_STRUCPP_SUBS) --input bin/main.so.substitutions

install: all
	ioc install --clean -V --ioc $(IOC_NAME)

clean:
	$(MAKE) -C src clean
	$(RM) $(IOC_STRUCPP_SUBS)
