PYBUILD_DIR := pybuild

all: prepare
	@echo "Invoking pybuild all $(MAKEFLAGS)"
	@PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=$$PWD python3 -m $(PYBUILD_DIR) $(MAKEFLAGS) all

prepare:
	@git submodule update --init --recursive

%:
	@PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=$$PWD python3 -m $(PYBUILD_DIR) $(MAKEFLAGS) $@ 

