// SPDX-License-Identifier: GPL-2.0-or-later

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/acpi.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#define CLEVO_ACPI_DSM_UUID	"93f224e4-fbdc-4bbf-add6-db71bdc0afad"

struct clevo_data {
	struct input_dev *input;
};

static const struct key_entry clevo_keymap[] = {
	// White-only KBLED
	{ KE_KEY, 0x20, { KEY_KBDILLUMDOWN } },
	{ KE_KEY, 0x21, { KEY_KBDILLUMUP } },
	{ KE_KEY, 0x3f, { KEY_KBDILLUMTOGGLE } },

	// RGB KBLED
	{ KE_KEY, 0x81, { KEY_KBDILLUMDOWN } },
	{ KE_KEY, 0x82, { KEY_KBDILLUMUP } },
	{ KE_KEY, 0x9f, { KEY_KBDILLUMTOGGLE } },

	{ KE_IGNORE, 0x7b },			// Fn+Backspace
	{ KE_IGNORE, 0x83 },			// Color cycle
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
	if (err)
		return err;

	err = input_register_device(input);
	if (err)
		return err;

	priv->input = input;
	return 0;
}

// Clevo DCHU SCMD expects a package with one integer element.
static bool clevo_dchu_cmd(acpi_handle handle, u8 method_id, u32 data)
{
	union acpi_object arg4;
	union acpi_object req;
	union acpi_object *obj;
	guid_t dsm_guid;

	req.type = ACPI_TYPE_INTEGER;
	req.integer.value = data;

	arg4.type = ACPI_TYPE_PACKAGE;
	arg4.package.count = 1;
	arg4.package.elements = &req;

	guid_parse(CLEVO_ACPI_DSM_UUID, &dsm_guid);
	obj = acpi_evaluate_dsm_typed(handle, &dsm_guid, 0, method_id, &arg4,
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

static void clevo_acpi_notify(acpi_handle handle, u32 event, void *context)
{
	struct clevo_data *priv = dev_get_drvdata(context);

	pr_info("event: %#x\n", event);

	if (!sparse_keymap_report_event(priv->input, event, 1, true))
		dev_dbg(context, "unknown key event: %#x\n", event);
}

static int clevo_acpi_probe(struct platform_device *pdev)
{
	struct clevo_data *priv;
	struct acpi_device *adev;
	int err;

	adev = ACPI_COMPANION(&pdev->dev);
	if (!adev)
		return -ENODEV;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);

	err = clevo_input_init(&pdev->dev);
	if (err)
		return err;

	err = acpi_dev_install_notify_handler(adev, ACPI_ALL_NOTIFY,
					      clevo_acpi_notify, &pdev->dev);
	if (err)
		return err;

	clevo_enable_notify_events(adev->handle);

	return 0;
}

static void clevo_acpi_remove(struct platform_device *pdev)
{
	acpi_dev_remove_notify_handler(ACPI_COMPANION(&pdev->dev),
				       ACPI_ALL_NOTIFY, clevo_acpi_notify);
}

static const struct acpi_device_id clevo_acpi_match[] = {
	{ "CLV0001", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, clevo_acpi_match);

static struct platform_driver clevo_acpi_driver = {
	.probe = clevo_acpi_probe,
	.remove = clevo_acpi_remove,
	.driver = {
		.name = "clevo-acpi",
		.acpi_match_table = clevo_acpi_match,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};
module_platform_driver(clevo_acpi_driver);

MODULE_DESCRIPTION("Clevo ACPI driver");
MODULE_LICENSE("GPL");
