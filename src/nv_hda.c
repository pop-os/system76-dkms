/*  Based on bbswitch,
 *  Copyright (C) 2011-2013 Bumblebee Project
 *  Author: Peter Wu <lekensteyn@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

enum {
    CARD_UNCHANGED = -1,
    CARD_OFF = 0,
    CARD_ON = 1,
};

static struct pci_dev *dis_dev;
static struct pci_dev *sub_dev = NULL;

// Returns 1 if the card is disabled, 0 if enabled
static int is_card_disabled(void) {
    //check for: 1.bit is set 2.sub-function is available.
    u32 cfg_word;
    struct pci_dev *tmp_dev = NULL;

    sub_dev = NULL;

    // read config word at 0x488
    pci_read_config_dword(dis_dev, 0x488, &cfg_word);
    if ((cfg_word & 0x2000000)==0x2000000) {
        //check for subdevice. read first config dword of sub function 1
        while ((tmp_dev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, tmp_dev)) != NULL) {
            int pci_class = tmp_dev->class >> 8;

            if (pci_class != 0x403)
                continue;

            if (tmp_dev->vendor == PCI_VENDOR_ID_NVIDIA) {
                sub_dev = tmp_dev;
                S76_INFO("Found NVIDIA audio device %s\n", dev_name(&tmp_dev->dev));
            }
        }

        if (sub_dev == NULL) {
            S76_INFO("No NVIDIA audio device found, unsetting config bit.\n");
            cfg_word|=0x2000000;
            pci_write_config_dword(dis_dev, 0x488, cfg_word);
            return 1;
        }

        return 0;
    } else {
        return 1;
    }
}

static void nv_hda_off(void) {
    u32 cfg_word;
    if (is_card_disabled()) {
        return;
    }

    //remove device
    pci_dev_put(sub_dev);
    pci_stop_and_remove_bus_device(sub_dev);

    S76_INFO("NVIDIA audio: disabling\n");

    //setting bit to turn off
    pci_read_config_dword(dis_dev, 0x488, &cfg_word);
    cfg_word&=0xfdffffff;
    pci_write_config_dword(dis_dev, 0x488, cfg_word);
}

static void nv_hda_on(void) {
    u32 cfg_word;
    u8 hdr_type;

    if (!is_card_disabled()) {
        return;
    }

    S76_INFO("NVIDIA audio: enabling\n");

    // read,set bit, write config word at 0x488
    pci_read_config_dword(dis_dev, 0x488, &cfg_word);
    cfg_word|=0x2000000;
    pci_write_config_dword(dis_dev, 0x488, cfg_word);

    //pci_scan_single_device
	pci_read_config_byte(dis_dev, PCI_HEADER_TYPE, &hdr_type);

	if (!(hdr_type & 0x80)) {
        S76_ERROR("NVIDIA not multifunction, no audio\n");
		return;
    }

	sub_dev = pci_scan_single_device(dis_dev->bus, 1);
	if (!sub_dev) {
        S76_ERROR("No NVIDIA audio device found\n");
		return;
    }

    S76_INFO("NVIDIA audio found, adding\n");
	pci_assign_unassigned_bus_resources(dis_dev->bus);
	pci_bus_add_devices(dis_dev->bus);
    pci_dev_get(sub_dev);
}

/* power bus so we can read PCI configuration space */
static void dis_dev_get(void) {
    if (dis_dev->bus && dis_dev->bus->self) {
        pm_runtime_get_sync(&dis_dev->bus->self->dev);
    }
}

static void dis_dev_put(void) {
    if (dis_dev->bus && dis_dev->bus->self) {
        pm_runtime_put_sync(&dis_dev->bus->self->dev);
    }
}


static int __init nv_hda_init(struct device *dev) {
    struct pci_dev *pdev = NULL;

    while ((pdev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, pdev)) != NULL) {
        int pci_class = pdev->class >> 8;

        if (pci_class != PCI_CLASS_DISPLAY_VGA && pci_class != PCI_CLASS_DISPLAY_3D) {
            continue;
        }

        if (pdev->vendor == PCI_VENDOR_ID_NVIDIA) {
            dis_dev = pdev;
            S76_INFO("NVIDIA device %s\n", dev_name(&pdev->dev));
        }
    }

    if (dis_dev == NULL) {
        S76_ERROR("No NVIDIA device found\n");
        return -ENODEV;
    }

    dis_dev_get();

    nv_hda_on();

    S76_INFO("NVIDIA Audio %s is %s\n", dev_name(&dis_dev->dev), is_card_disabled() ? "off" : "on");

    dis_dev_put();

    return 0;
}

static void __exit nv_hda_exit(void) {
    if (dis_dev == NULL)
        return;

    dis_dev_get();

    nv_hda_off();

    pr_info("NVIDIA Audio %s is %s\n", dev_name(&dis_dev->dev), is_card_disabled() ? "off" : "on");

    dis_dev_put();

}
