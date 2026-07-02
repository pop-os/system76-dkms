# SPDX-License-Identifier: GPL-2.0-or-later

obj-m := src/

KDIR := /lib/modules/$(shell uname -r)/build

build:
	make -C $(KDIR) M=$(CURDIR)

clean:
	make -C $(KDIR) M=$(CURDIR) clean
