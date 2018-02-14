#!/bin/bash

modprobe acpi_call

if lsmod | grep -q acpi_call; then
methods="
\_SB.PCI0.PEG0.PEGP._OFF
"

for m in $methods; do
    echo -n "Trying $m: "
    echo $m > /proc/acpi/call
    result=$(cat /proc/acpi/call)
    case "$result" in
        Error*)
            echo "failed"
        ;;
        *)
            echo "works!"
            # break # try out outher methods too
        ;;
    esac
done

else
    echo "The acpi_call module is not loaded, try running 'modprobe acpi_call' or 'insmod acpi_call.ko' as root"
    exit 1
fi
