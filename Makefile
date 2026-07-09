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
		--show-types \
		--ignore LINUX_VERSION_CODE,CONSTANT_COMPARISON \
		src/clevo-acpi.c
