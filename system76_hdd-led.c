/*
 * hdd_led.c
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

static enum led_brightness hdd_led_brightness = 1;

static enum led_brightness hdd_led_get(struct led_classdev *led_cdev) {
	return hdd_led_brightness;
}

static int hdd_led_set(struct led_classdev *led_cdev, enum led_brightness value) {
    S76_INFO("hdd_led_set %d\n", (int)value);

    if (value > 0) {
        s76_wmbb(0x79, 0x090000FF, NULL);
    } else {
        s76_wmbb(0x79, 0x09000000, NULL);
    }

	return 0;
}

static struct led_classdev hdd_led = {
   .name = "system76::hdd",
   .brightness_get = hdd_led_get,
   .brightness_set_blocking = hdd_led_set,
   .max_brightness = 1,
   .default_trigger = "disk-activity"
};

static void hdd_led_resume(void) {
	hdd_led_set(&hdd_led, hdd_led_brightness);
}

static int __init hdd_led_init(struct device *dev) {
	int err;

	err = led_classdev_register(dev, &hdd_led);
	if (unlikely(err)) {
		return err;
	}

	hdd_led_resume();

	return 0;
}

static void __exit hdd_led_exit(void) {
	if (!IS_ERR_OR_NULL(hdd_led.dev)) {
		led_classdev_unregister(&hdd_led);
	}
}
