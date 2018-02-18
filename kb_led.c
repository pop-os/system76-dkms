/*
 * kb_led.c
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

#define SET_KB_LED 0x67

static enum led_brightness kb_led_brightness = LED_OFF;

static enum led_brightness kb_led_get(struct led_classdev *led_cdev) {
	return kb_led_brightness;
}

static int kb_led_set(struct led_classdev *led_cdev, enum led_brightness value) {
	if (!s76_wmbb(SET_KB_LED, 0xF4000000 | value, NULL)) {
		kb_led_brightness = value;
	}
	
	return 0;
}

static struct led_classdev kb_led = {
	.name = "system76::kbd_backlight",
	.flags = LED_BRIGHT_HW_CHANGED,
	.brightness_get = kb_led_get,
	.brightness_set_blocking = kb_led_set,
	.max_brightness = 255,
};

static int __init kb_led_init(struct device *dev) {
	int err;

	err = led_classdev_register(dev, &kb_led);
	if (unlikely(err)) {
		return err;
	}

	return 0;
}

static void __exit kb_led_exit(void) {
	if (!IS_ERR_OR_NULL(kb_led.dev)) {
		led_classdev_unregister(&kb_led);
	}
}
