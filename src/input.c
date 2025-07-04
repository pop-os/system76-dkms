// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * input.c
 *
 * Copyright (C) 2017 Jeremy Soller <jeremy@system76.com>
 * Copyright (C) 2014-2016 Arnoud Willemsen <mail@lynthium.com>
 * Copyright (C) 2013-2015 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
 */

#define AIRPLANE_KEY KEY_WLAN
#define SCREEN_KEY KEY_SCREENLOCK

static struct input_dev *s76_input_device;
static DEFINE_MUTEX(s76_input_report_mutex);

#define POLL_FREQ_MIN     1
#define POLL_FREQ_MAX     20
#define POLL_FREQ_DEFAULT 5

static int param_set_poll_freq(const char *val, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_byte(val, kp);

	if (!ret)
		*((unsigned char *) kp->arg) = clamp_t(unsigned char,
			*((unsigned char *) kp->arg),
			POLL_FREQ_MIN, POLL_FREQ_MAX);

	return ret;
}


static const struct kernel_param_ops param_ops_poll_freq = {
	.set = param_set_poll_freq,
	.get = param_get_byte,
};

static unsigned char param_poll_freq = POLL_FREQ_DEFAULT;
#define param_check_poll_freq param_check_byte
module_param_named(poll_freq, param_poll_freq, poll_freq, 0400);
MODULE_PARM_DESC(poll_freq, "Set polling frequency");

static struct task_struct *s76_input_polling_task;

static void s76_input_key(unsigned int code)
{
	pr_debug("Send key %x\n", code);

	mutex_lock(&s76_input_report_mutex);

	input_report_key(s76_input_device, code, 1);
	input_sync(s76_input_device);

	input_report_key(s76_input_device, code, 0);
	input_sync(s76_input_device);

	mutex_unlock(&s76_input_report_mutex);
}

static int s76_input_polling_thread(void *data)
{
	pr_debug("Polling thread started (PID: %i), polling at %i Hz\n",
				current->pid, param_poll_freq);

	while (!kthread_should_stop()) {
		u8 byte;

		ec_read(0xDB, &byte);
		if (byte & 0x40) {
			ec_write(0xDB, byte & ~0x40);

			pr_debug("Airplane-Mode Hotkey pressed (EC)\n");

			s76_input_key(AIRPLANE_KEY);
		}

		msleep_interruptible(1000 / param_poll_freq);
	}

	pr_debug("Polling thread exiting\n");

	return 0;
}

static void s76_input_airplane_wmi(void)
{
	pr_debug("Airplane-Mode Hotkey pressed (WMI)\n");

	s76_input_key(AIRPLANE_KEY);
}

static void s76_input_screen_wmi(void)
{
	pr_debug("Screen Hotkey pressed (WMI)\n");

	s76_input_key(SCREEN_KEY);
}

static int s76_input_open(struct input_dev *dev)
{
	int res = 0;

	// Run polling thread if AP key driver is used and WMI is not supported
	if ((driver_flags & (DRIVER_AP_KEY | DRIVER_AP_WMI)) == DRIVER_AP_KEY) {
		s76_input_polling_task = kthread_run(
			s76_input_polling_thread,
			NULL, "system76-polld");

		if (IS_ERR(s76_input_polling_task)) {
			res = PTR_ERR(s76_input_polling_task);
			s76_input_polling_task = NULL;
			pr_err("Could not create polling thread: %d\n", res);
			return res;
		}
	}

	return res;
}

static void s76_input_close(struct input_dev *dev)
{
	if (IS_ERR_OR_NULL(s76_input_polling_task)) {
		return;
	}

	kthread_stop(s76_input_polling_task);
	s76_input_polling_task = NULL;
}

static int __init s76_input_init(struct device *dev)
{
	u8 byte;

	s76_input_device = devm_input_allocate_device(dev);
	if (!s76_input_device) {
		pr_err("Error allocating input device\n");
		return -ENOMEM;
	}

	s76_input_device->name = "System76 Hotkeys";
	s76_input_device->phys = "system76/input0";
	s76_input_device->id.bustype = BUS_HOST;
	set_bit(EV_KEY, s76_input_device->evbit);

	if (driver_flags & DRIVER_AP_KEY) {
		set_bit(AIRPLANE_KEY, s76_input_device->keybit);
		ec_read(0xDB, &byte);
		ec_write(0xDB, byte & ~0x40);
	}
	if (driver_flags & DRIVER_OLED) {
		set_bit(SCREEN_KEY, s76_input_device->keybit);
	}

	s76_input_device->open  = s76_input_open;
	s76_input_device->close = s76_input_close;

	return input_register_device(s76_input_device);
}
