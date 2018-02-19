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

union kb_led_color {
	u32 rgb;
	struct { u32 b:8, g:8, r:8, : 8; };
};

enum kb_led_region {
	KB_LED_REGION_LEFT,
    KB_LED_REGION_CENTER,
    KB_LED_REGION_RIGHT,
    KB_LED_REGION_EXTRA,
};

static enum led_brightness kb_led_brightness = 0;

static enum led_brightness kb_led_toggle_brightness = 72;

static enum led_brightness kb_led_levels[] = { 48, 72, 96, 144, 192, 255 };

static union kb_led_color kb_led_regions[] = {
	{ .rgb = 0xFFFFFF },
	{ .rgb = 0xFFFFFF },
	{ .rgb = 0xFFFFFF },
	{ .rgb = 0xFFFFFF }
};

static int kb_led_colors_i = 0;

static union kb_led_color kb_led_colors[] = {
	{ .rgb = 0xFFFFFF },
	{ .rgb = 0x0000FF },
	{ .rgb = 0xFF0000 },
	{ .rgb = 0xFF00FF },
	{ .rgb = 0x00FF00 },
	{ .rgb = 0x00FFFF },
	{ .rgb = 0xFFFF00 }
};

static enum led_brightness kb_led_get(struct led_classdev *led_cdev) {
	return kb_led_brightness;
}

static int kb_led_set(struct led_classdev *led_cdev, enum led_brightness value) {
	S76_INFO("kb_led_set %d\n", (int)value);
	
	if (!s76_wmbb(SET_KB_LED, 0xF4000000 | value, NULL)) {
		kb_led_brightness = value;
	}
	
	return 0;
}

static void kb_led_color_set(enum kb_led_region region, union kb_led_color color) {
	u32 cmd;
	
	S76_INFO("kb_led_color_set %d %x\n", (int)region, (int)color.rgb);
	
	switch (region) {
	case KB_LED_REGION_LEFT:
		cmd = 0xF0000000;
		break;
	case KB_LED_REGION_CENTER:
		cmd = 0xF1000000;
		break;
	case KB_LED_REGION_RIGHT:
		cmd = 0xF2000000;
		break;
	case KB_LED_REGION_EXTRA:
		cmd = 0xF3000000;
		break;
	default:
		return;
	}
	
	cmd |= color.b << 16;
	cmd |= color.r <<  8;
	cmd |= color.g <<  0;

	if (!s76_wmbb(SET_KB_LED, cmd, NULL)) {
		kb_led_regions[region] = color;
	}
}

static struct led_classdev kb_led = {
	.name = "system76::kbd_backlight",
	.flags = LED_BRIGHT_HW_CHANGED,
	.brightness_get = kb_led_get,
	.brightness_set_blocking = kb_led_set,
	.max_brightness = 255,
};

static void kb_led_resume(void) {
	enum kb_led_region region;
	
	// Enable keyboard backlight
	s76_wmbb(SET_KB_LED, 0xE007F001, NULL);
	
	// Reset current brightness
	kb_led_set(&kb_led, kb_led_brightness);
	
	// Reset current color
	for (region = 0; region < sizeof(kb_led_regions)/sizeof(union kb_led_color); region++) {
		kb_led_color_set(region, kb_led_regions[region]);
	}
}

static int __init kb_led_init(struct device *dev) {
	int err;

	err = led_classdev_register(dev, &kb_led);
	if (unlikely(err)) {
		return err;
	}

	kb_led_resume();

	return 0;
}

static void __exit kb_led_exit(void) {
	if (!IS_ERR_OR_NULL(kb_led.dev)) {
		led_classdev_unregister(&kb_led);
	}
}

static void kb_wmi_brightness(enum led_brightness value) {
	S76_INFO("kb_wmi_brightness %d\n", (int)value);
	
	kb_led_set(&kb_led, value);
	led_classdev_notify_brightness_hw_changed(&kb_led, value);
}

static void kb_wmi_toggle(void) {
	if (kb_led_brightness > 0) {
		kb_led_toggle_brightness = kb_led_brightness;
		kb_wmi_brightness(LED_OFF);
	} else {
		kb_wmi_brightness(kb_led_toggle_brightness);
	}
}

static void kb_wmi_dec(void) {
	int i;
	
	if (kb_led_brightness > 0) {
		for (i = sizeof(kb_led_levels)/sizeof(enum led_brightness); i > 0; i--) {
			if (kb_led_levels[i - 1] < kb_led_brightness) {
				kb_wmi_brightness(kb_led_levels[i - 1]);
				break;
			}
		}
	} else {
		kb_wmi_toggle();
	}
}

static void kb_wmi_inc(void) {
	int i;

	if (kb_led_brightness > 0) {
		for (i = 0; i < sizeof(kb_led_levels)/sizeof(enum led_brightness); i++) {
			if (kb_led_levels[i] > kb_led_brightness) {
				kb_wmi_brightness(kb_led_levels[i]);
				break;
			}
		}
	} else {
		kb_wmi_toggle();
	}
}

static void kb_wmi_color(void) {
	enum kb_led_region region;
	
	kb_led_colors_i += 1;
	if (kb_led_colors_i >= sizeof(kb_led_colors)/sizeof(union kb_led_color)) {
		kb_led_colors_i = 0;
	}
	
	for (region = 0; region < sizeof(kb_led_regions)/sizeof(union kb_led_color); region++) {
		kb_led_color_set(region, kb_led_colors[kb_led_colors_i]);
	}
}
