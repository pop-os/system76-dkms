/*
 * led.c
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

static struct workqueue_struct *kb_led_workqueue;

static struct _kb_led_work {
	struct work_struct work;
	int wk;
} kb_led_work;

static void kb_led_update(struct work_struct *work) {
	u8 byte;
	struct _kb_led_work *w;

	w = container_of(work, struct _kb_led_work, work);
	//
	// ec_read(0xD9, &byte);
	//
	// ec_write(0xD9, w->wk ? byte & ~0x40 : byte | 0x40);
	//
	// /* wmbb 0x6C 1 (?) */
}

static enum led_brightness kb_led_get(struct led_classdev *led_cdev) {
	// u8 byte;
	//
	// ec_read(0xD9, &byte);
	//
	// return byte & 0x40 ? LED_OFF : LED_FULL;

	return LED_OFF;
}

/* must not sleep */
static void kb_led_set(struct led_classdev *led_cdev, enum led_brightness value) {
	kb_led_work.wk = value;
	queue_work(kb_led_workqueue, &kb_led_work.work);
}

static struct led_classdev kb_led = {
	.name = "system76::kbd_backlight",
	.flags = LED_BRIGHT_HW_CHANGED,
	.brightness_get = kb_led_get,
	.brightness_set = kb_led_set,
	.max_brightness = 3,
};

static int __init kb_led_init(struct device *dev) {
	int err;

	kb_led_workqueue = create_singlethread_workqueue("kb_led_workqueue");
	if (unlikely(!kb_led_workqueue)) {
		return -ENOMEM;
	}

	INIT_WORK(&kb_led_work.work, kb_led_update);

	err = led_classdev_register(dev, &kb_led);
	if (unlikely(err)) {
		goto err_destroy_workqueue;
	}

	return 0;

err_destroy_workqueue:
	destroy_workqueue(kb_led_workqueue);
	kb_led_workqueue = NULL;

	return err;
}

static void __exit kb_led_exit(void) {
	if (!IS_ERR_OR_NULL(kb_led.dev))
		led_classdev_unregister(&kb_led);
	if (kb_led_workqueue)
		destroy_workqueue(kb_led_workqueue);
}
