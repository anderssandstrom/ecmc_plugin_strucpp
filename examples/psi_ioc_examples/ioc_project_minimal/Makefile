.PHONY: all install clean

all:
	$(MAKE) -C src stage

install: all
	ioc install --source .

clean:
	$(MAKE) -C src clean
