// SPDX-License-Identifier: GPL-2.0-or-later

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/version.h>

// Clevo DCHU DSM UUID: "93f224e4-fbdc-4bbf-add6-db71bdc0afad"
static const guid_t dchu_dsm_guid =
	GUID_INIT(0x93f224e4, 0xfbdc, 0x4bbf,
		  0xad, 0xd6, 0xdb, 0x71, 0xbd, 0xc0, 0xaf, 0xad);

static const enum led_brightness kb_led_levels[] = {
	48,
	72,
	96,
	144,
	192,
	255,
};

static const u32 kb_led_colors[] = {
	0xFFFFFF, // WHITE
	0x0000FF, // BLUE
	0xFF0000, // RED
	0xFF00FF, // MAGENTA
	0x00FF00, // GREEN
	0x00FFFF, // CYAN
	0xFFFF00, // YELLOW
};

struct clevo_data {
	struct platform_device *pdev;
	struct input_dev *input;
	struct led_classdev kb_led;
	u8 kb_brightness;
	u8 kb_toggle_brightness;
	u32 kb_color_index;
	u8 kbd_type;
};

static const struct key_entry clevo_keymap[] = {
	// White-only KBD
	{ KE_KEY, 0x20, { KEY_KBDILLUMDOWN } },
	{ KE_KEY, 0x21, { KEY_KBDILLUMUP } },
	{ KE_KEY, 0x3f, { KEY_KBDILLUMTOGGLE } },

	// RGB KBD
	{ KE_KEY, 0x81, { KEY_KBDILLUMDOWN } },
	{ KE_KEY, 0x82, { KEY_KBDILLUMUP } },
	{ KE_IGNORE, 0x83 },			// Color cycle
	{ KE_KEY, 0x9f, { KEY_KBDILLUMTOGGLE } },

	{ KE_IGNORE, 0x7b },			// Fn+Backspace
	{ KE_IGNORE, 0x8f },			// Fan max on (Fn+1)
	{ KE_IGNORE, 0x95 },			// Fn+Esc
	{ KE_IGNORE, 0xf6 },			// Camera disable
	{ KE_IGNORE, 0xf7 },			// Camera enable
	{ KE_IGNORE, 0xfa },			// Speaker volume change
	{ KE_IGNORE, 0xfb },			// Speaker mute toggle
	{ KE_IGNORE, 0xfc, { KEY_F21 } },	// Touchpad disable
	{ KE_IGNORE, 0xfd, { KEY_F21 } },	// Touchpad enable
	{ KE_END }
};

static acpi_status clevo_ec_cmd(u8 *input, size_t input_length,
				u8 *output, size_t output_length)
{
	struct acpi_buffer out = { ACPI_ALLOCATE_BUFFER, NULL };
	struct acpi_object_list in;
	union acpi_object obj;
	acpi_handle handle;
	acpi_status status;

	if (!input || input_length != 8)
		return AE_BAD_PARAMETER;
	if (output && output_length != 6)
		return AE_BAD_PARAMETER;

	obj.type = ACPI_TYPE_BUFFER;
	obj.buffer.length = input_length;
	obj.buffer.pointer = input;

	in.count = 1;
	in.pointer = &obj;

	// FIXME: Get from HID "PNP0C09"
	status = acpi_get_handle(NULL, (acpi_string)"\\_SB.PC00.LPCB.EC", &handle);
	if (ACPI_FAILURE(status)) {
		pr_err("failed to get EC handle: %#x\n", status);
		return status;
	}

	status = acpi_evaluate_object(handle, "ECMD", &in, &out);
	if (ACPI_FAILURE(status)) {
		pr_err("failed to call ECMD: %#x\n", status);
		return status;
	}

	// ECMD always returns a 6-byte buffer.
	union acpi_object *ret_obj __free(kfree) = out.pointer;

	if (output) {
		if (!ret_obj)
			return AE_ERROR;

		if (ret_obj->type != ACPI_TYPE_BUFFER)
			return AE_ERROR;

		memcpy(output, ret_obj->buffer.pointer, output_length);
	}

	return AE_OK;
}

static void clevo_ec_kbd_color_set(u32 color)
{
	u8 buf[8] = {};
	const u8 zones[] = {
		0x03, // Left
		0x04, // Middle
		0x05, // Right
		0x0B, // Numpad
		0x07, // Lightbar
	};

	buf[0] = 5; // Payload size
	buf[2] = 0xCA; // Command
	buf[4] = color & 0xFF; // Blue
	buf[5] = (color >> 16) & 0xFF; // Red
	buf[6] = (color >> 8) & 0xFF; // Green

	for (int i = 0;  i < ARRAY_SIZE(zones); i++) {
		buf[3] = zones[i];
		(void)clevo_ec_cmd(buf, ARRAY_SIZE(buf), NULL, 0);
	}
}

static void clevo_ec_kbd_brightness_set(enum led_brightness value)
{
	u8 buf[8] = {};

	buf[0] = 5; // Payload size
	buf[2] = 0xCA; // Command
	buf[3] = 0x06; // Brightness
	buf[4] = value; // KBD
	buf[6] = value; // Lightbar

	(void)clevo_ec_cmd(buf, ARRAY_SIZE(buf), NULL, 0);
}

// TODO: Return an enum.
// Clevo only reported 2 possible values (1, 6), and serw14 returns 0x17.
static u8 clevo_dchu_kbd_type(acpi_handle handle)
{
	union acpi_object arg4;
	union acpi_object *obj;
	u8 kbd_type = 0;
	u8 buf[256] = {};

	arg4.type = ACPI_TYPE_BUFFER;
	arg4.buffer.length = sizeof(buf);
	arg4.buffer.pointer = buf;

	obj = acpi_evaluate_dsm_typed(handle, &dchu_dsm_guid, 0, 0x0d, &arg4,
				      ACPI_TYPE_BUFFER);
	if (obj) {
		kbd_type = obj->buffer.pointer[0x0f];
		ACPI_FREE(obj);
	}

	return kbd_type;
}

// Clevo DCHU SCMD expects a package with one integer element.
static bool clevo_dchu_cmd(acpi_handle handle, u8 method_id, u32 data)
{
	union acpi_object arg4;
	union acpi_object req;
	union acpi_object *obj;

	req.type = ACPI_TYPE_INTEGER;
	req.integer.value = data;

	arg4.type = ACPI_TYPE_PACKAGE;
	arg4.package.count = 1;
	arg4.package.elements = &req;

	obj = acpi_evaluate_dsm_typed(handle, &dchu_dsm_guid, 0, method_id, &arg4,
				      ACPI_TYPE_INTEGER);

	if (obj) {
		// SCMD returns the method ID on success
		bool ret = obj->integer.value == method_id;

		ACPI_FREE(obj);
		return ret;
	}

	return false;
}

// Sets HKDR=1 so NEVT will Notify device
static bool clevo_enable_notify_events(acpi_handle handle)
{
	return clevo_dchu_cmd(handle, 0x46, 0);
}

static enum led_brightness clevo_kbled_get(struct led_classdev *led_cdev)
{
	struct clevo_data *priv;

	priv = container_of(led_cdev, struct clevo_data, kb_led);

	return priv->kb_brightness;
}

static int clevo_kbled_set(struct led_classdev *led_cdev,
			   enum led_brightness brightness)
{
	struct clevo_data *priv;
	struct acpi_device *adev;

	pr_debug("%s %d\n", __func__, (int)brightness);

	priv = container_of(led_cdev, struct clevo_data, kb_led);
	adev = ACPI_COMPANION(&priv->pdev->dev);

	priv->kb_brightness = brightness;

	if (priv->kbd_type == 1)
		clevo_dchu_cmd(adev->handle, 0x27, priv->kb_brightness);
	else
		clevo_ec_kbd_brightness_set(priv->kb_brightness);

	return 0;
}

static void kbled_hotkey_toggle(struct clevo_data *priv)
{
	if (priv->kb_brightness > 0) {
		priv->kb_toggle_brightness = priv->kb_brightness;
		priv->kb_brightness = 0;
	} else {
		priv->kb_brightness = priv->kb_toggle_brightness;
	}

	clevo_kbled_set(&priv->kb_led, priv->kb_brightness);
	led_classdev_notify_brightness_hw_changed(&priv->kb_led, priv->kb_brightness);
}

static void kbled_hotkey_white_dec(struct clevo_data *priv)
{
	if (priv->kb_brightness > 0)
		priv->kb_brightness--;

	clevo_kbled_set(&priv->kb_led, priv->kb_brightness);
	led_classdev_notify_brightness_hw_changed(&priv->kb_led, priv->kb_brightness);
}

static void kbled_hotkey_rgb_dec(struct clevo_data *priv)
{
	if (priv->kb_brightness > 0) {
		for (int i = ARRAY_SIZE(kb_led_levels); i > 0; i--) {
			if (kb_led_levels[i - 1] < priv->kb_brightness) {
				priv->kb_brightness = kb_led_levels[i - 1];
				clevo_kbled_set(&priv->kb_led, priv->kb_brightness);
				break;
			}
		}
	} else {
		clevo_kbled_set(&priv->kb_led, priv->kb_toggle_brightness);
	}

	led_classdev_notify_brightness_hw_changed(&priv->kb_led, priv->kb_brightness);
}

static void kbled_hotkey_white_inc(struct clevo_data *priv)
{
	if (priv->kb_brightness < 5)
		priv->kb_brightness++;

	clevo_kbled_set(&priv->kb_led, priv->kb_brightness);
	led_classdev_notify_brightness_hw_changed(&priv->kb_led, priv->kb_brightness);
}

static void kbled_hotkey_rgb_inc(struct clevo_data *priv)
{
	if (priv->kb_brightness > 0) {
		for (int i = 0; i < ARRAY_SIZE(kb_led_levels); i++) {
			if (kb_led_levels[i] > priv->kb_brightness) {
				priv->kb_brightness = kb_led_levels[i];
				clevo_kbled_set(&priv->kb_led, priv->kb_brightness);
				break;
			}
		}
	} else {
		clevo_kbled_set(&priv->kb_led, priv->kb_toggle_brightness);
	}

	led_classdev_notify_brightness_hw_changed(&priv->kb_led, priv->kb_brightness);
}

static void kbled_hotkey_rgb_color(struct clevo_data *priv)
{
	priv->kb_color_index += 1;
	if (priv->kb_color_index >= ARRAY_SIZE(kb_led_colors))
		priv->kb_color_index = 0;

	clevo_ec_kbd_color_set(kb_led_colors[priv->kb_color_index]);

	led_classdev_notify_brightness_hw_changed(&priv->kb_led, priv->kb_brightness);
}

static int clevo_kbled_init(struct device *dev)
{
	struct clevo_data *priv = dev_get_drvdata(dev);
	struct acpi_device *adev = ACPI_COMPANION(dev);
	struct led_init_data init_data = {
		.devicename = "clevo-acpi",
		.default_label = ":" LED_FUNCTION_KBD_BACKLIGHT,
		.devname_mandatory = true,
	};
	int err;

	priv->kbd_type = clevo_dchu_kbd_type(adev->handle);

	if (priv->kbd_type == 1) {
		pr_debug("white-only KBLED\n");
		priv->kb_toggle_brightness = 2;
		priv->kb_led.max_brightness = 5;
	} else {
		pr_debug("RGB KBLED\n");
		priv->kb_toggle_brightness = 72;
		priv->kb_led.max_brightness = 255;
	}

	priv->kb_color_index = 0;
	priv->kb_brightness = priv->kb_toggle_brightness;

	priv->kb_led.brightness = priv->kb_brightness;
	priv->kb_led.brightness_set_blocking = clevo_kbled_set;
	priv->kb_led.brightness_get = clevo_kbled_get;
	// XXX: Other flags?
	priv->kb_led.flags = LED_BRIGHT_HW_CHANGED |
			     LED_REJECT_NAME_CONFLICT;

	err = devm_led_classdev_register_ext(dev, &priv->kb_led, &init_data);
	if (err)
		return err;

	clevo_dchu_cmd(adev->handle, 0x67, 0xE007F001);
	clevo_kbled_set(&priv->kb_led, priv->kb_brightness);
	if (priv->kbd_type != 1)
		clevo_ec_kbd_color_set(kb_led_colors[priv->kb_color_index]);

	return 0;
}

static int clevo_input_init(struct device *dev)
{
	struct clevo_data *priv = dev_get_drvdata(dev);
	struct input_dev *input;
	int err;

	input = devm_input_allocate_device(dev);
	if (!input)
		return -ENOMEM;

	input->name = "Clevo ACPI hotkeys";
	input->phys = "clevo-acpi/input0";
	input->id.bustype = BUS_HOST;

	err = sparse_keymap_setup(input, clevo_keymap, NULL);
	if (err) {
		pr_err("failed to set up input device keymap\n");
		return err;
	}

	err = input_register_device(input); // devres managed
	if (err) {
		pr_err("failed to register input device\n");
		return err;
	}

	priv->input = input;

	return 0;
}

static void clevo_acpi_notify(acpi_handle handle, u32 event, void *context)
{
	struct clevo_data *priv = dev_get_drvdata(context);

	pr_debug("event: %#x\n", event);

	switch (event) {
	case 0x20:
		kbled_hotkey_white_dec(priv);
		break;
	case 0x21:
		kbled_hotkey_white_inc(priv);
		break;

	case 0x81:
		kbled_hotkey_rgb_dec(priv);
		break;
	case 0x82:
		kbled_hotkey_rgb_inc(priv);
		break;
	case 0x83:
		if (priv->kbd_type != 1)
			kbled_hotkey_rgb_color(priv);
		break;

	case 0x3f:
	case 0x9f:
		kbled_hotkey_toggle(priv);
		break;
	}

	if (!sparse_keymap_report_event(priv->input, event, 1, true))
		pr_warn("unknown key event: %#x\n", event);
}

static int clevo_acpi_suspend(struct device *dev)
{
	dev_dbg(dev, "suspend\n");

	return 0;
}

static int clevo_acpi_resume(struct device *dev)
{
	struct clevo_data *priv = dev_get_drvdata(dev);
	struct acpi_device *adev = ACPI_COMPANION(dev);

	dev_dbg(dev, "resume\n");

	clevo_enable_notify_events(adev->handle);

	// FIXME: This fixes turning KBLED back on for some reason.
	// Even on White-only KBLED.
	clevo_ec_kbd_color_set(kb_led_colors[priv->kb_color_index]);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
static DEFINE_SIMPLE_DEV_PM_OPS(clevo_acpi_pm, clevo_acpi_suspend, clevo_acpi_resume);
#else
static SIMPLE_DEV_PM_OPS(clevo_acpi_pm, clevo_acpi_suspend, clevo_acpi_resume);
#endif

// TODO: Model-specific quirks
#define SYSTEM76_DMI(version) { \
		.matches = { \
			DMI_MATCH(DMI_SYS_VENDOR, "System76"), \
			DMI_MATCH(DMI_PRODUCT_VERSION, version), \
		}, \
	}

// XXX: Limit functionality to latest models while drivers are being reworked.
static const struct dmi_system_id system76_dmi_table[] = {
	SYSTEM76_DMI("addp6"),
	SYSTEM76_DMI("lemp14"),
	SYSTEM76_DMI("lemp14-b"),
	SYSTEM76_DMI("oryp14"),
	// For testing; model has lightbar
	//SYSTEM76_DMI("serw14"),
	{ }

};
MODULE_DEVICE_TABLE(dmi, system76_dmi_table);

static int clevo_acpi_probe(struct platform_device *pdev)
{
	struct clevo_data *priv;
	struct acpi_device *adev;
	int err;

	dev_dbg(&pdev->dev, "probe\n");

	adev = ACPI_COMPANION(&pdev->dev);
	if (!adev)
		return -ENODEV;

	if (!dmi_check_system(system76_dmi_table)) {
		pr_info("model does not utilize this driver\n");
		return -ENODEV;
	}

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->pdev = pdev;

	err = clevo_input_init(&pdev->dev);
	if (err)
		return err;

	err = clevo_kbled_init(&pdev->dev);
	if (err)
		return err;

	// TODO: Use `devm_acpi_install_notify_handler` when available.
	err = acpi_dev_install_notify_handler(adev, ACPI_ALL_NOTIFY,
					      clevo_acpi_notify, &pdev->dev);
	if (err)
		return err;

	clevo_enable_notify_events(adev->handle);

	return 0;
}

static void clevo_acpi_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "remove\n");

	acpi_dev_remove_notify_handler(ACPI_COMPANION(&pdev->dev),
				       ACPI_ALL_NOTIFY, clevo_acpi_notify);
}

static const struct acpi_device_id clevo_acpi_ids[] = {
	{ "CLV0001", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, clevo_acpi_ids);

static struct platform_driver clevo_acpi_driver = {
	.probe = clevo_acpi_probe,
	.remove = clevo_acpi_remove,
	.driver = {
		.name = "clevo-acpi",
		.acpi_match_table = clevo_acpi_ids,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
		.pm = pm_sleep_ptr(&clevo_acpi_pm),
#else
		.pm = pm_ptr(&s76_pm),
#endif
	},
};
module_platform_driver(clevo_acpi_driver);

MODULE_DESCRIPTION("Clevo ACPI driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1.0");
