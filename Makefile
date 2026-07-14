# SPDX-License-Identifier: GPL-2.0-or-later

KDIR = /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C "$(KDIR)" M="$(PWD)" modules

clean:
	$(MAKE) -C "$(KDIR)" M="$(PWD)" clean

checkpatch:
	$(KDIR)/scripts/checkpatch.pl \
		--no-tree \
		--file \
		--strict \
		--show-types \
		--ignore LINUX_VERSION_CODE,CONSTANT_COMPARISON,MACRO_ARG_UNUSED,MACRO_ARG_REUSE,LONG_LINE \
		src/clevo-acpi.c \
		src/ap-led.c \
		src/kb-led.c \
		src/input.c \
		src/hwmon.c \
		src/system76.c
