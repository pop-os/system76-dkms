// SPDX-License-Identifier: GPL-2.0-or-later

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/acpi.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#define CLEVO_ACPI_DSM_UUID	"93f224e4-fbdc-4bbf-add6-db71bdc0afad"

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
	pr_info("event: %#x\n", event);
}

static int clevo_acpi_probe(struct platform_device *pdev)
{
	struct acpi_device *adev;
	int err;

	adev = ACPI_COMPANION(&pdev->dev);
	if (!adev)
		return -ENODEV;

	err = acpi_dev_install_notify_handler(adev, ACPI_DEVICE_NOTIFY,
					      clevo_acpi_notify, &pdev->dev);
	if (err)
		return err;

	clevo_enable_notify_events(adev->handle);

	return 0;
}

static void clevo_acpi_remove(struct platform_device *pdev)
{
	acpi_dev_remove_notify_handler(ACPI_COMPANION(&pdev->dev),
				       ACPI_DEVICE_NOTIFY, clevo_acpi_notify);
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
