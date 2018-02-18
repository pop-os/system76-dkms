/*
 * ap_led.c
 *
 * Copyright (C) 2017 Jeremy Soller <jeremy@system76.com>
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

static enum led_brightness ap_led_get(struct led_classdev *led_cdev) {
	u8 byte;

	ec_read(0xD9, &byte);

	return byte & 0x40 ? LED_OFF : LED_FULL;
}

static int ap_led_set(struct led_classdev *led_cdev, enum led_brightness value) {
	u8 byte;
	
	ec_read(0xD9, &byte);
	ec_write(0xD9, value ? byte & ~0x40 : byte | 0x40);
	
	return 0;
}

static struct led_classdev ap_led = {
	.name = "system76::airplane",
	.brightness_get = ap_led_get,
	.brightness_set_blocking = ap_led_set,
	.max_brightness = 1,
	.default_trigger = "rfkill-any"
};

static int __init ap_led_init(struct device *dev) {
	int err;

	err = led_classdev_register(dev, &ap_led);
	if (unlikely(err)) {
		return err;
	}

	return 0;
}

static void __exit ap_led_exit(void) {
	if (!IS_ERR_OR_NULL(ap_led.dev)) {
		led_classdev_unregister(&ap_led);
	}
}
