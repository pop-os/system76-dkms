#!/bin/bash

set -ex

dh clean
make clean

make
make clean

debuild -b -uc -us
dh clean

sudo dpkg -i ../system76-dkms_0.0.1_amd64.deb

sudo modprobe -r system76 || true
sudo modprobe system76

dmesg -w | grep system76
