// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * system76.c
 *
 * Copyright (C) 2017 Jeremy Soller <jeremy@system76.com>
 * Copyright (C) 2014-2016 Arnoud Willemsen <mail@lynthium.com>
 * Copyright (C) 2013-2015 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
 */

#define S76_DRIVER_NAME KBUILD_MODNAME
#define pr_fmt(fmt) S76_DRIVER_NAME ": " fmt

#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/dmi.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/i8042.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reboot.h>
#include <linux/rfkill.h>
#include <linux/stringify.h>
#include <linux/version.h>
#include <linux/workqueue.h>

#define S76_EVENT_GUID  "ABBC0F6B-8EA1-11D1-00A0-C90629100000"
#define S76_WMBB_GUID    "ABBC0F6D-8EA1-11D1-00A0-C90629100000"

#define S76_HAS_HWMON (defined(CONFIG_HWMON) || (defined(MODULE) && defined(CONFIG_HWMON_MODULE)))

/* method IDs for S76_GET */
#define GET_EVENT               0x01  /*   1 */

#define DRIVER_AP_KEY		(1 << 0)
#define DRIVER_AP_LED		(1 << 1)
#define DRIVER_HWMON		(1 << 2)
#define DRIVER_KB_LED_WMI	(1 << 3)
#define DRIVER_OLED		(1 << 4)
#define DRIVER_AP_WMI		(1 << 5)
#define DRIVER_KB_LED		(1 << 6)

#define DRIVER_INPUT  (DRIVER_AP_KEY | DRIVER_OLED)

static uint64_t driver_flags;

struct platform_device *s76_platform_device;

static int s76_wmbb(u32 method_id, u32 arg, u32 *retval)
{
	struct acpi_buffer in  = { (acpi_size) sizeof(arg), &arg };
	struct acpi_buffer out = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;
	acpi_status status;
	u32 tmp;

	pr_debug("%0#4x  IN : %0#6x\n", method_id, arg);

	status = wmi_evaluate_method(S76_WMBB_GUID, 0, method_id, &in, &out);

	if (unlikely(ACPI_FAILURE(status))) {
		return -EIO;
	}

	obj = (union acpi_object *) out.pointer;
	if (obj && obj->type == ACPI_TYPE_INTEGER) {
		tmp = (u32) obj->integer.value;
	} else {
		tmp = 0;
	}

	pr_debug("%0#4x  OUT: %0#6x (IN: %0#6x)\n", method_id, tmp, arg);

	if (likely(retval)) {
		*retval = tmp;
	}

	kfree(obj);

	return 0;
}

#include "ap-led.c"
#include "input.c"
#include "kb-led.c"
#include "hwmon.c"
#include "nv_hda.c"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
static void s76_wmi_notify(union acpi_object *obj, void *context)
#else
static void s76_wmi_notify(u32 value, void *context)
#endif
{
	u32 event;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
	if (obj->type != ACPI_TYPE_INTEGER) {
		pr_debug("Unexpected WMI event (%0#6x)\n", obj->type);
		return;
	}
#else
	if (value != 0xD0) {
		pr_debug("Unexpected WMI event (%0#6x)\n", value);
		return;
	}
#endif

	s76_wmbb(GET_EVENT, 0, &event);

	pr_debug("WMI event code (%x)\n", event);

	switch (event) {
	case 0x81:
		if (driver_flags & (DRIVER_KB_LED_WMI | DRIVER_KB_LED)) {
			kb_wmi_dec();
		}
		break;
	case 0x82:
		if (driver_flags & (DRIVER_KB_LED_WMI | DRIVER_KB_LED)) {
			kb_wmi_inc();
		}
		break;
	case 0x83:
		if (driver_flags & (DRIVER_KB_LED_WMI | DRIVER_KB_LED)) {
			kb_wmi_color();
		}
		break;
	case 0x7b:
		//TODO: Fn+Backspace
		break;
	case 0x95:
		//TODO: Fn+ESC
		break;
	case 0x9F:
		if (driver_flags & (DRIVER_KB_LED_WMI | DRIVER_KB_LED)) {
			kb_wmi_toggle();
		}
		break;
	case 0xD7:
		if (driver_flags & DRIVER_OLED) {
			s76_input_screen_wmi();
		}
		break;
	case 0x85:
	case 0xF4:
		if (driver_flags & DRIVER_AP_KEY) {
			s76_input_airplane_wmi();
		}
		break;
	case 0xFC:
		// Touchpad WMI (disable)
		break;
	case 0xFD:
		// Touchpad WMI (enable)
		break;
	default:
		pr_debug("Unknown WMI event code (%x)\n", event);
		break;
	}
}

static int __init s76_probe(struct platform_device *dev)
{
	int err;

	if (driver_flags & DRIVER_AP_LED) {
		err = ap_led_init(&dev->dev);
		if (unlikely(err)) {
			pr_err("Could not register LED device\n");
		}
	}

	if (driver_flags & (DRIVER_KB_LED_WMI | DRIVER_KB_LED)) {
		err = kb_led_init(&dev->dev);
		if (unlikely(err)) {
			pr_err("Could not register LED device\n");
		}
	}

	if (driver_flags & DRIVER_INPUT) {
		err = s76_input_init(&dev->dev);
		if (unlikely(err)) {
			pr_err("Could not register input device\n");
		}
	}

#ifdef S76_HAS_HWMON
	if (driver_flags & DRIVER_HWMON) {
		s76_hwmon_init(&dev->dev);
	}
#endif

	err = nv_hda_init(&dev->dev);
	if (unlikely(err)) {
		pr_err("Could not register NVIDIA audio device\n");
	}

	err = wmi_install_notify_handler(S76_EVENT_GUID, s76_wmi_notify, NULL);
	if (unlikely(ACPI_FAILURE(err))) {
		pr_err("Could not register WMI notify handler (%0#6x)\n", err);
		return -EIO;
	}

	// Enable hotkey support
	s76_wmbb(0x46, 0, NULL);

	// Enable touchpad lock
	i8042_lock_chip();
	i8042_command(NULL, 0x97);
	i8042_unlock_chip();

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void s76_remove(struct platform_device *dev)
#else
static int s76_remove(struct platform_device *dev)
#endif
{
	wmi_remove_notify_handler(S76_EVENT_GUID);

	nv_hda_exit();
	#ifdef S76_HAS_HWMON
	if (driver_flags & DRIVER_HWMON) {
		s76_hwmon_fini(&dev->dev);
	}
	#endif
	if (driver_flags & (DRIVER_KB_LED_WMI | DRIVER_KB_LED)) {
		kb_led_exit();
	}
	if (driver_flags & DRIVER_AP_LED) {
		ap_led_exit();
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
	return 0;
#endif
}

static int s76_suspend(struct device *dev)
{
	pr_debug("%s\n", __func__);

	if (driver_flags & (DRIVER_KB_LED_WMI | DRIVER_KB_LED)) {
		kb_led_suspend();
	}

	return 0;
}

static int s76_resume(struct device *dev)
{
	pr_debug("%s\n", __func__);

	msleep(2000);

	if (driver_flags & DRIVER_AP_LED) {
		ap_led_resume();
	}
	if (driver_flags & (DRIVER_KB_LED_WMI | DRIVER_KB_LED)) {
		kb_led_resume();
	}

	// Enable hotkey support
	s76_wmbb(0x46, 0, NULL);

	// Enable touchpad lock
	i8042_lock_chip();
	i8042_command(NULL, 0x97);
	i8042_unlock_chip();

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
static DEFINE_SIMPLE_DEV_PM_OPS(s76_pm, s76_suspend, s76_resume);
#else
static SIMPLE_DEV_PM_OPS(s76_pm, s76_suspend, s76_resume);
#endif

static struct platform_driver s76_platform_driver = {
	.remove = s76_remove,
	.driver = {
		.name  = S76_DRIVER_NAME,
		.owner = THIS_MODULE,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
		.pm = pm_sleep_ptr(&s76_pm),
#else
		.pm = pm_ptr(&s76_pm),
#endif
	},
};

static int __init s76_dmi_matched(const struct dmi_system_id *id)
{
	pr_info("Model %s found\n", id->ident);
	driver_flags = (uint64_t)id->driver_data;
	return 1;
}

#define DMI_TABLE_LEGACY(PRODUCT, DATA) { \
	.ident = "System76 " PRODUCT, \
	.matches = { \
		DMI_MATCH(DMI_SYS_VENDOR, "System76"), \
		DMI_MATCH(DMI_PRODUCT_VERSION, PRODUCT), \
		DMI_MATCH(DMI_BIOS_VENDOR, "System76"), \
	}, \
	.callback = s76_dmi_matched, \
	.driver_data = (void *)(uint64_t)0, \
}

#define DMI_TABLE(PRODUCT, DATA) { \
	.ident = "System76 " PRODUCT, \
	.matches = { \
		DMI_MATCH(DMI_SYS_VENDOR, "System76"), \
		DMI_MATCH(DMI_PRODUCT_VERSION, PRODUCT), \
	}, \
	.callback = s76_dmi_matched, \
	.driver_data = (void *)(uint64_t)(DATA), \
}

static struct dmi_system_id s76_dmi_table[] __initdata = {
	DMI_TABLE_LEGACY("bonw13", DRIVER_HWMON | DRIVER_KB_LED_WMI),
	DMI_TABLE("addw1", DRIVER_AP_LED | DRIVER_KB_LED_WMI | DRIVER_OLED),
	DMI_TABLE("addw2", DRIVER_AP_LED | DRIVER_KB_LED_WMI | DRIVER_OLED),
	DMI_TABLE("addw5", DRIVER_HWMON | DRIVER_KB_LED_WMI),
	DMI_TABLE("bonw15-b", DRIVER_HWMON | DRIVER_KB_LED_WMI),
	DMI_TABLE("bonw16", DRIVER_HWMON | DRIVER_KB_LED_WMI),
	DMI_TABLE("darp5", DRIVER_AP_LED | DRIVER_HWMON | DRIVER_KB_LED_WMI),
	DMI_TABLE("darp6", DRIVER_AP_LED | DRIVER_HWMON | DRIVER_KB_LED_WMI),
	DMI_TABLE("galp2", DRIVER_HWMON),
	DMI_TABLE("galp3", DRIVER_HWMON),
	DMI_TABLE("galp3-b", DRIVER_AP_KEY | DRIVER_AP_LED | DRIVER_HWMON),
	DMI_TABLE("galp3-c", DRIVER_AP_LED | DRIVER_HWMON),
	DMI_TABLE("galp4", DRIVER_AP_LED | DRIVER_HWMON),
	DMI_TABLE("gaze13", DRIVER_AP_KEY | DRIVER_AP_LED | DRIVER_HWMON),
	DMI_TABLE("gaze14", DRIVER_AP_LED | DRIVER_KB_LED_WMI),
	DMI_TABLE("gaze15", DRIVER_AP_LED | DRIVER_KB_LED_WMI),
	DMI_TABLE("kudu5", DRIVER_AP_KEY | DRIVER_AP_LED | DRIVER_HWMON),
	DMI_TABLE("kudu6", DRIVER_AP_KEY | DRIVER_AP_WMI | DRIVER_KB_LED_WMI),
	DMI_TABLE("oryp3-jeremy", DRIVER_AP_KEY | DRIVER_AP_LED | DRIVER_HWMON | DRIVER_KB_LED_WMI),
	DMI_TABLE("oryp4", DRIVER_AP_KEY | DRIVER_AP_LED | DRIVER_HWMON | DRIVER_KB_LED_WMI),
	DMI_TABLE("oryp4-b", DRIVER_AP_KEY | DRIVER_AP_LED | DRIVER_HWMON | DRIVER_KB_LED_WMI),
	DMI_TABLE("oryp5", DRIVER_AP_LED | DRIVER_HWMON | DRIVER_KB_LED_WMI),
	DMI_TABLE("oryp6", DRIVER_AP_LED | DRIVER_KB_LED_WMI),
	DMI_TABLE("pang10", DRIVER_AP_KEY | DRIVER_AP_WMI | DRIVER_KB_LED_WMI),
	DMI_TABLE("pang11", DRIVER_AP_KEY | DRIVER_AP_WMI | DRIVER_KB_LED_WMI),
	DMI_TABLE("serw11", DRIVER_AP_KEY | DRIVER_AP_LED | DRIVER_HWMON | DRIVER_KB_LED_WMI),
	DMI_TABLE("serw11-b", DRIVER_AP_KEY | DRIVER_AP_LED | DRIVER_HWMON | DRIVER_KB_LED_WMI),
	DMI_TABLE("serw12", DRIVER_AP_KEY | DRIVER_AP_LED | DRIVER_AP_WMI | DRIVER_KB_LED_WMI),
	DMI_TABLE("serw14", DRIVER_HWMON | DRIVER_KB_LED),
	{}
};
MODULE_DEVICE_TABLE(dmi, s76_dmi_table);

static int __init s76_init(void)
{
	if (!dmi_check_system(s76_dmi_table)) {
		pr_info("Model does not utilize this driver");
		return -ENODEV;
	}

	if (!driver_flags) {
		pr_info("Driver data not defined");
		return -ENODEV;
	}

	if (!wmi_has_guid(S76_EVENT_GUID)) {
		pr_info("No known WMI event notification GUID found\n");
		return -ENODEV;
	}

	if (!wmi_has_guid(S76_WMBB_GUID)) {
		pr_info("No known WMI control method GUID found\n");
		return -ENODEV;
	}

	s76_platform_device =
		platform_create_bundle(&s76_platform_driver, s76_probe, NULL, 0, NULL, 0);

	if (IS_ERR(s76_platform_device)) {
		return PTR_ERR(s76_platform_device);
	}

	return 0;
}
module_init(s76_init);

static void __exit s76_exit(void)
{
	platform_device_unregister(s76_platform_device);
	platform_driver_unregister(&s76_platform_driver);
}
module_exit(s76_exit);

MODULE_AUTHOR("Jeremy Soller <jeremy@system76.com>");
MODULE_DESCRIPTION("System76 laptop driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
