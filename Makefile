MODULE=system76
VERSION=0.1

all:
	-make uninstall
	make install

reload:
	-make remove
	make && make insert || cat /var/lib/dkms/$(MODULE)/$(VERSION)/build/make.log

install:
	sudo dkms install $(PWD)/$(MODULE) --force

uninstall:
	sudo dkms remove $(MODULE)/$(VERSION) --all

insert:
	sudo modprobe $(MODULE)

remove:
	sudo modprobe -r $(MODULE)
