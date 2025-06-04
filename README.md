# system76-dkms
System76 DKMS driver

On newer System76 laptops, this driver controls some of the hotkeys and allows for custom fan control.

## Development

To install this as a kernel module:

```
# Compile the module
make
# Remove any old instances
sudo modprobe -r system76
# Insert the new module
sudo insmod src/system76.ko
# View log messages
dmesg | grep system76
```
