debuild -b -uc -us
sudo dpkg -i ../system76-dkms_0.0.1_amd64.deb
cat /var/lib/dkms/system76/0.0.1/build/make.log
sudo modprobe -r system76
sudo modprobe system76
dmesg -w | grep system76
