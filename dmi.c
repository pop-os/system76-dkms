/*
 * dmi.c
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

static int __init s76_dmi_matched(const struct dmi_system_id *id)
{
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
	.driver_data = &DATA, \
}

static struct dmi_system_id s76_dmi_table[] __initdata = {
	DMI_TABLE("bonw13", kb_full_color_with_extra_ops),
	{}
};

MODULE_DEVICE_TABLE(dmi, s76_dmi_table);