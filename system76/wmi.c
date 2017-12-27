/*
 * wmi.c
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

static void s76_wmi_notify(u32 value, void *context)
{
	u32 event;

	if (value != 0xD0) {
		S76_INFO("Unexpected WMI event (%0#6x)\n", value);
		return;
	}

	s76_wmi_evaluate_wmbb_method(GET_EVENT, 0, &event);
	
	S76_INFO("WMI event code (%x)\n", event);

	switch (event) {
	case 0xF4:
		airplane_wmi();
		break;
	default:
		kb_wmi(event);
		break;
	}
}

static int s76_wmi_probe(struct platform_device *dev)
{
	int status;

	status = wmi_install_notify_handler(S76_EVENT_GUID,
		s76_wmi_notify, NULL);
	if (unlikely(ACPI_FAILURE(status))) {
		S76_ERROR("Could not register WMI notify handler (%0#6x)\n",
			status);
		return -EIO;
	}

	if (kb_backlight.ops)
		kb_backlight.ops->init();

	return 0;
}

static int s76_wmi_remove(struct platform_device *dev)
{
	wmi_remove_notify_handler(S76_EVENT_GUID);
	return 0;
}

static int s76_wmi_resume(struct platform_device *dev)
{
	if (kb_backlight.ops && kb_backlight.state == KB_STATE_ON)
		kb_backlight.ops->set_mode(kb_backlight.mode);

	return 0;
}

static struct platform_driver s76_platform_driver = {
	.remove = s76_wmi_remove,
	.resume = s76_wmi_resume,
	.driver = {
		.name  = S76_DRIVER_NAME,
		.owner = THIS_MODULE,
	},
};