PYBUILD_DIR := pybuild

all: prepare
	@echo "Invoking pybuild all $(MAKEFLAGS)"
	@PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=$$PWD python -m $(PYBUILD_DIR) $(MAKEFLAGS) all

prepare:
	echo @git submodule update --init --recursive $(PYBUILD_DIR)

%:
	@PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=$$PWD python -m $(PYBUILD_DIR) $(MAKEFLAGS) $@ 

