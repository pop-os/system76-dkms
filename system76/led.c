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

static bool param_led_invert;
module_param_named(led_invert, param_led_invert, bool, 0);
MODULE_PARM_DESC(led_invert, "Invert airplane mode LED state.");

static struct workqueue_struct *led_workqueue;

static struct _led_work {
	struct work_struct work;
	int wk;
} led_work;

static void airplane_led_update(struct work_struct *work)
{
	u8 byte;
	struct _led_work *w;

	w = container_of(work, struct _led_work, work);

	ec_read(0xD9, &byte);

	if (param_led_invert)
		ec_write(0xD9, w->wk ? byte & ~0x40 : byte | 0x40);
	else
		ec_write(0xD9, w->wk ? byte | 0x40 : byte & ~0x40);

	/* wmbb 0x6C 1 (?) */
}

static enum led_brightness airplane_led_get(struct led_classdev *led_cdev)
{
	u8 byte;

	ec_read(0xD9, &byte);

	if (param_led_invert)
		return byte & 0x40 ? LED_OFF : LED_FULL;
	else
		return byte & 0x40 ? LED_FULL : LED_OFF;
}

/* must not sleep */
static void airplane_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	S76_INFO("Set airplane LED to %X", value);
	led_work.wk = value;
	queue_work(led_workqueue, &led_work.work);
}

static struct led_classdev airplane_led = {
	.name = "system76::airplane",
	.brightness_get = airplane_led_get,
	.brightness_set = airplane_led_set,
	.max_brightness = 1,
	.default_trigger = "rfkill-any"
};

static int __init s76_led_init(void)
{
	int err;
	
	param_led_invert = TRUE;

	led_workqueue = create_singlethread_workqueue("led_workqueue");
	if (unlikely(!led_workqueue))
		return -ENOMEM;

	INIT_WORK(&led_work.work, airplane_led_update);

	err = led_classdev_register(&s76_platform_device->dev,
		&airplane_led);
	if (unlikely(err))
		goto err_destroy_workqueue;

	return 0;

err_destroy_workqueue:
	destroy_workqueue(led_workqueue);
	led_workqueue = NULL;

	return err;
}

static void __exit s76_led_exit(void)
{
	if (!IS_ERR_OR_NULL(airplane_led.dev))
		led_classdev_unregister(&airplane_led);
	if (led_workqueue)
		destroy_workqueue(led_workqueue);
}