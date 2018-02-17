/*
 * system76.c
 *
 * Copyright (C) 2017 Jeremy Soller <jeremy@system76.com>
 * Copyright (C) 2014-2016 Arnoud Willemsen <mail@lynthium.com>
 * Copyright (C) 2013-2015 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the  GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is  distributed in the hope that it  will be useful, but
 * WITHOUT  ANY   WARRANTY;  without   even  the  implied   warranty  of
 * MERCHANTABILITY  or FITNESS FOR  A PARTICULAR  PURPOSE.  See  the GNU
 * General Public License for more details.
 *
 * You should  have received  a copy of  the GNU General  Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
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
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/stringify.h>
#include <linux/version.h>
#include <linux/workqueue.h>

#define __S76_PR(lvl, fmt, ...) do { pr_##lvl(fmt, ##__VA_ARGS__); } \
		while (0)
#define S76_INFO(fmt, ...) __S76_PR(info, fmt, ##__VA_ARGS__)
#define S76_ERROR(fmt, ...) __S76_PR(err, fmt, ##__VA_ARGS__)
#define S76_DEBUG(fmt, ...) __S76_PR(debug, "[%s:%u] " fmt, \
		__func__, __LINE__, ##__VA_ARGS__)

#define S76_EVENT_GUID  "ABBC0F6B-8EA1-11D1-00A0-C90629100000"
#define S76_WMBB_GUID    "ABBC0F6D-8EA1-11D1-00A0-C90629100000"

#define S76_HAS_HWMON (defined(CONFIG_HWMON) || (defined(MODULE) && defined(CONFIG_HWMON_MODULE)))

/* method IDs for S76_GET */
#define GET_EVENT               0x01  /*   1 */

struct platform_device *s76_platform_device;

static int s76_wmbb(u32 method_id, u32 arg, u32 *retval) {
	struct acpi_buffer in  = { (acpi_size) sizeof(arg), &arg };
	struct acpi_buffer out = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;
	acpi_status status;
	u32 tmp;

	S76_DEBUG("%0#4x  IN : %0#6x\n", method_id, arg);

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

	S76_DEBUG("%0#4x  OUT: %0#6x (IN: %0#6x)\n", method_id, tmp, arg);

	if (likely(retval)) {
		*retval = tmp;
	}

	kfree(obj);

	return 0;
}

#include "ec.c"
#include "ap_led.c"
#include "input.c"
#include "kb_led.c"
#include "kb.c"
#include "hwmon.c"

static void s76_debug_wmi(void) {
	S76_INFO("Debug WMI\n");

	u32 val = 0;

	#define DEBUG_WMI(N, V) { \
		s76_wmbb(V, 0, &val); \
		S76_INFO("%s %x = %x\n", N, V, val); \
	}

	DEBUG_WMI("?", 0x01);
	DEBUG_WMI("A2", 0x05);
	DEBUG_WMI("Webcam", 0x06);
	DEBUG_WMI("Bluetooth", 0x07);
	DEBUG_WMI("Touchpad", 0x09);
	DEBUG_WMI("A4", 0x0A);
	DEBUG_WMI("?", 0x10);
	DEBUG_WMI("A7", 0x11);
	DEBUG_WMI("?", 0x12);
	DEBUG_WMI("?", 0x32);
	DEBUG_WMI("?", 0x33);
	DEBUG_WMI("?", 0x34);
	DEBUG_WMI("?", 0x38);
	DEBUG_WMI("?", 0x39);
	DEBUG_WMI("?", 0x3B);
	DEBUG_WMI("?", 0x3C);
	DEBUG_WMI("Keyboard Backlight", 0x3D);
	DEBUG_WMI("?", 0x3F);
	DEBUG_WMI("?", 0x40);
	DEBUG_WMI("?", 0x41);
	DEBUG_WMI("?", 0x42);
	DEBUG_WMI("?", 0x43);
	DEBUG_WMI("?", 0x44);
	DEBUG_WMI("?", 0x45);
	DEBUG_WMI("?", 0x51);
	DEBUG_WMI("?", 0x52);
	DEBUG_WMI("VGA", 0x54);
	DEBUG_WMI("?", 0x62);
	DEBUG_WMI("?", 0x63);
	DEBUG_WMI("?", 0x64);
	DEBUG_WMI("?", 0x6E);
	DEBUG_WMI("?", 0x6F);
	DEBUG_WMI("?", 0x70);
	DEBUG_WMI("?", 0x71);
	DEBUG_WMI("?", 0x73);
	DEBUG_WMI("?", 0x77);
	DEBUG_WMI("?", 0x7A);
}

static void s76_wmi_notify(u32 value, void *context) {
	u32 event;

	if (value != 0xD0) {
		S76_INFO("Unexpected WMI event (%0#6x)\n", value);
		return;
	}

	s76_wmbb(GET_EVENT, 0, &event);

	S76_INFO("WMI event code (%x)\n", event);

	switch (event) {
	case 0x95:
		s76_debug_wmi();
		break;
	case 0xF4:
		s76_input_airplane_wmi();
		break;
	case 0xFC:
		s76_input_touchpad_wmi(false);
		break;
	case 0xFD:
		s76_input_touchpad_wmi(true);
		break;
	default:
		kb_wmi(event);
		break;
	}
}

static int s76_probe(struct platform_device *dev) {
	int err;

	err = ec_init();
	if (unlikely(err)) {
		S76_ERROR("Could not register EC device\n");
	}

	err = ap_led_init(&dev->dev);
	if (unlikely(err)) {
		S76_ERROR("Could not register LED device\n");
	}

	err = kb_led_init(&dev->dev);
	if (unlikely(err)) {
		S76_ERROR("Could not register LED device\n");
	}

	err = s76_input_init(&dev->dev);
	if (unlikely(err)) {
		S76_ERROR("Could not register input device\n");
	}

	if (device_create_file(&dev->dev, &dev_attr_kb_brightness) != 0) {
		S76_ERROR("Sysfs attribute creation failed for brightness\n");
	}

	if (device_create_file(&dev->dev, &dev_attr_kb_state) != 0) {
		S76_ERROR("Sysfs attribute creation failed for state\n");
	}

	if (device_create_file(&dev->dev, &dev_attr_kb_mode) != 0) {
		S76_ERROR("Sysfs attribute creation failed for mode\n");
	}

	if (device_create_file(&dev->dev, &dev_attr_kb_color) != 0) {
		S76_ERROR("Sysfs attribute creation failed for color\n");
	}

	if (kb_backlight.ops) {
		kb_backlight.ops->init();
	}

#ifdef S76_HAS_HWMON
	s76_hwmon_init(&dev->dev);
#endif

	err = wmi_install_notify_handler(S76_EVENT_GUID, s76_wmi_notify, NULL);
	if (unlikely(ACPI_FAILURE(err))) {
		S76_ERROR("Could not register WMI notify handler (%0#6x)\n", err);
		return -EIO;
	}

	// Enable hotkey support
	s76_wmbb(0x46, 0, NULL);

	// Enable touchpad lock
	//i8042_lock_chip();
	//i8042_command(NULL, 0x97);
	//i8042_unlock_chip();

	return 0;
}

static int s76_remove(struct platform_device *dev) {
	wmi_remove_notify_handler(S76_EVENT_GUID);

	#ifdef S76_HAS_HWMON
		s76_hwmon_fini(&dev->dev);
	#endif

	device_remove_file(&dev->dev, &dev_attr_kb_color);
	device_remove_file(&dev->dev, &dev_attr_kb_mode);
	device_remove_file(&dev->dev, &dev_attr_kb_state);
	device_remove_file(&dev->dev, &dev_attr_kb_brightness);

	s76_input_exit();
	kb_led_exit();
	ap_led_exit();

	ec_exit();

	return 0;
}

static int s76_resume(struct platform_device *dev) {
	// Enable hotkey support
	s76_wmbb(0x46, 0, NULL);

	if (kb_backlight.ops && kb_backlight.state == KB_STATE_ON) {
		kb_backlight.ops->set_mode(kb_backlight.mode);
	}

	return 0;
}

static struct platform_driver s76_platform_driver = {
	.remove = s76_remove,
	.resume = s76_resume,
	.driver = {
		.name  = S76_DRIVER_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init s76_dmi_matched(const struct dmi_system_id *id) {
	S76_INFO("Model %s found\n", id->ident);
	kb_backlight.ops = id->driver_data;

	return 1;
}

#define DMI_TABLE(PRODUCT, DATA) { \
	.ident = "System76 " PRODUCT, \
	.matches = { \
		DMI_MATCH(DMI_SYS_VENDOR, "System76"), \
		DMI_MATCH(DMI_PRODUCT_VERSION, PRODUCT), \
	}, \
	.callback = s76_dmi_matched, \
	.driver_data = DATA, \
}

static struct dmi_system_id s76_dmi_table[] __initdata = {
	DMI_TABLE("oryp3-jeremy", &kb_full_color_ops),
	{}
};

MODULE_DEVICE_TABLE(dmi, s76_dmi_table);

static int __init s76_init(void) {
	int err;

	switch (param_kb_color_num) {
	case 1:
		param_kb_color[1] = param_kb_color[2] = param_kb_color[0] = param_kb_color[3];
		break;
	case 2:
		return -EINVAL;
	}

	dmi_check_system(s76_dmi_table);

	if (!wmi_has_guid(S76_EVENT_GUID)) {
		S76_INFO("No known WMI event notification GUID found\n");
		return -ENODEV;
	}

	if (!wmi_has_guid(S76_WMBB_GUID)) {
		S76_INFO("No known WMI control method GUID found\n");
		return -ENODEV;
	}

	s76_platform_device =
		platform_create_bundle(&s76_platform_driver, s76_probe, NULL, 0, NULL, 0);

	if (unlikely(IS_ERR(s76_platform_device))) {
		return PTR_ERR(s76_platform_device);
	}

	return 0;
}

static void __exit s76_exit(void) {
	platform_device_unregister(s76_platform_device);
	platform_driver_unregister(&s76_platform_driver);
}

module_init(s76_init);
module_exit(s76_exit);

MODULE_AUTHOR("Jeremy Soller <jeremy@system76.com>");
MODULE_DESCRIPTION("System76 laptop driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
