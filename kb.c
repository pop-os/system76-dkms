/*
 * kb.c
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

#define SET_KB_LED 0x67  /* 103 */

#define COLORS { \
	C(white,   0xFFFFFF), \
	C(blue,    0x0000FF), \
	C(red,     0xFF0000), \
	C(magenta, 0xFF00FF), \
	C(green,   0x00FF00), \
	C(cyan,    0x00FFFF), \
	C(yellow,  0xFFFF00), \
}
#undef C


#define C(n, v) KB_COLOR_##n
enum kb_color COLORS;
#undef C

union kb_rgb_color {
	u32 rgb;
	struct { u32 b:8, g:8, r:8, : 8; };
};

#define C(n, v) { .name = #n, .value = { .rgb = v, }, }
struct {
	const char *const name;
	union kb_rgb_color value;
} kb_colors[] = COLORS;
#undef C

#define KB_COLOR_DEFAULT      KB_COLOR_white
#define KB_BRIGHTNESS_MAX     3
#define KB_BRIGHTNESS_DEFAULT 0

static int param_set_kb_color(const char *val, const struct kernel_param *kp)
{
	size_t i;

	if (!val)
		return -EINVAL;

	if (!val[0]) {
		*((enum kb_color *) kp->arg) = KB_COLOR_DEFAULT;
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(kb_colors); i++) {
		if (!strcmp(val, kb_colors[i].name)) {
			*((enum kb_color *) kp->arg) = i;
			return 0;
		}
	}

	return -EINVAL;
}

static int param_get_kb_color(char *buffer, const struct kernel_param *kp)
{
	return sprintf(buffer, "%s",
		kb_colors[*((enum kb_color *) kp->arg)].name);
}

static const struct kernel_param_ops param_ops_kb_color = {
	.set = param_set_kb_color,
	.get = param_get_kb_color,
};

static enum kb_color param_kb_color[] = { [0 ... 3] = KB_COLOR_DEFAULT };
static int param_kb_color_num;
#define param_check_kb_color(name, p) __param_check(name, p, enum kb_color)
module_param_array_named(kb_color, param_kb_color, kb_color,
						 &param_kb_color_num, S_IRUSR);
MODULE_PARM_DESC(kb_color, "Set the color(s) of the keyboard (sections)");

static int param_set_kb_brightness(const char *val,
	const struct kernel_param *kp)
{
	int ret;

	ret = param_set_byte(val, kp);

	if (!ret && *((unsigned char *) kp->arg) > KB_BRIGHTNESS_MAX)
		return -EINVAL;

	return ret;
}

static const struct kernel_param_ops param_ops_kb_brightness = {
	.set = param_set_kb_brightness,
	.get = param_get_byte,
};

static unsigned char param_kb_brightness = KB_BRIGHTNESS_DEFAULT;
#define param_check_kb_brightness param_check_byte
module_param_named(kb_brightness, param_kb_brightness, kb_brightness, S_IRUSR);
MODULE_PARM_DESC(kb_brightness, "Set the brightness of the keyboard backlight");


static bool param_kb_off = true;
module_param_named(kb_off, param_kb_off, bool, S_IRUSR);
MODULE_PARM_DESC(kb_off, "Switch keyboard backlight off");

static bool param_kb_cycle_colors = true;
module_param_named(kb_cycle_colors, param_kb_cycle_colors, bool, S_IRUSR);
MODULE_PARM_DESC(kb_cycle_colors, "Cycle colors rather than modes");

static struct {
	enum kb_extra {
		KB_HAS_EXTRA_TRUE,
		KB_HAS_EXTRA_FALSE,
	} extra;

	enum kb_state {
		KB_STATE_OFF,
		KB_STATE_ON,
	} state;

	struct {
		unsigned left;
		unsigned center;
		unsigned right;
		unsigned extra;
	} color;

	unsigned brightness;

	enum kb_mode {
		KB_MODE_RANDOM_COLOR,
		KB_MODE_CUSTOM,
		KB_MODE_BREATHE,
		KB_MODE_CYCLE,
		KB_MODE_WAVE,
		KB_MODE_DANCE,
		KB_MODE_TEMPO,
		KB_MODE_FLASH,
	} mode;

	struct kb_backlight_ops {
		void (*set_state)(enum kb_state state);
		void (*set_color)(unsigned left, unsigned center,
			unsigned right, unsigned extra);
		void (*set_brightness)(unsigned brightness);
		void (*set_mode)(enum kb_mode);
		void (*init)(void);
	} *ops;

} kb_backlight = { .ops = NULL, };


static void kb_dec_brightness(void)
{
	if (kb_backlight.state == KB_STATE_OFF)
		return;
	if (kb_backlight.brightness == 0)
		return;

	S76_DEBUG();

	kb_backlight.ops->set_brightness(kb_backlight.brightness - 1);
}

static void kb_inc_brightness(void)
{
	if (kb_backlight.state == KB_STATE_OFF)
		return;

	S76_DEBUG();

	kb_backlight.ops->set_brightness(kb_backlight.brightness + 1);
}

static void kb_toggle_state(void)
{
	switch (kb_backlight.state) {
	case KB_STATE_OFF:
		kb_backlight.ops->set_state(KB_STATE_ON);
		break;
	case KB_STATE_ON:
		kb_backlight.ops->set_state(KB_STATE_OFF);
		break;
	default:
		BUG();
	}
}

static void kb_next_mode(void)
{
	static enum kb_mode modes[] = {
		KB_MODE_RANDOM_COLOR,
		KB_MODE_DANCE,
		KB_MODE_TEMPO,
		KB_MODE_FLASH,
		KB_MODE_WAVE,
		KB_MODE_BREATHE,
		KB_MODE_CYCLE,
		KB_MODE_CUSTOM,
	};

	size_t i;

	if (kb_backlight.state == KB_STATE_OFF)
		return;

	for (i = 0; i < ARRAY_SIZE(modes); i++) {
		if (modes[i] == kb_backlight.mode)
			break;
	}

	BUG_ON(i == ARRAY_SIZE(modes));

	kb_backlight.ops->set_mode(modes[(i + 1) % ARRAY_SIZE(modes)]);
}

static void kb_next_color(void)
{
	size_t i;
	unsigned int nc;

	if (kb_backlight.state == KB_STATE_OFF)
		return;

	for (i = 0; i < ARRAY_SIZE(kb_colors); i++) {
		if (i == kb_backlight.color.left)
			break;
	}

	if (i + 1 >= ARRAY_SIZE(kb_colors))
		nc = 0;
	else
		nc = i + 1;

	kb_backlight.ops->set_color(nc, nc, nc, nc);
}

/* full color backlight keyboard */

static void kb_full_color__set_color(unsigned left, unsigned center,
	unsigned right, unsigned extra)
{
	u32 cmd;

	S76_INFO(
		"Left: %s (%X), Center: %s (%X), Right: %s (%X), Extra: %s (%X)\n",
		kb_colors[left].name, (unsigned int)kb_colors[left].value.rgb,
		kb_colors[center].name, (unsigned int)kb_colors[center].value.rgb,
		kb_colors[right].name, (unsigned int)kb_colors[right].value.rgb,
		kb_colors[extra].name, (unsigned int)kb_colors[extra].value.rgb
	);

	ec_kb_color_set(EC_KB_LEFT, kb_colors[left].value.rgb);
	kb_backlight.color.left = left;

	ec_kb_color_set(EC_KB_CENTER, kb_colors[center].value.rgb);
	kb_backlight.color.center = center;

	ec_kb_color_set(EC_KB_RIGHT, kb_colors[right].value.rgb);
	kb_backlight.color.right = right;

	if (kb_backlight.extra == KB_HAS_EXTRA_TRUE) {
		ec_kb_color_set(EC_KB_EXTRA, kb_colors[extra].value.rgb);
		kb_backlight.color.extra = extra;
	}

	kb_backlight.mode = KB_MODE_CUSTOM;
}

static void kb_full_color__set_brightness(unsigned i)
{
	u8 lvl_to_raw[] = { 63, 126, 189, 252 };

	i = clamp_t(unsigned, i, 0, ARRAY_SIZE(lvl_to_raw) - 1);

	if (!s76_wmbb(SET_KB_LED,
		0xF4000000 | lvl_to_raw[i], NULL))
		kb_backlight.brightness = i;
}

static void kb_full_color__set_mode(unsigned mode)
{
	static u32 cmds[] = {
		[KB_MODE_BREATHE]      = 0x1002a000,
		[KB_MODE_CUSTOM]       = 0,
		[KB_MODE_CYCLE]        = 0x33010000,
		[KB_MODE_DANCE]        = 0x80000000,
		[KB_MODE_FLASH]        = 0xA0000000,
		[KB_MODE_RANDOM_COLOR] = 0x70000000,
		[KB_MODE_TEMPO]        = 0x90000000,
		[KB_MODE_WAVE]         = 0xB0000000,
	};

	BUG_ON(mode >= ARRAY_SIZE(cmds));

	s76_wmbb(SET_KB_LED, 0x10000000, NULL);

	if (mode == KB_MODE_CUSTOM) {
		kb_full_color__set_color(kb_backlight.color.left,
			kb_backlight.color.center,
			kb_backlight.color.right,
			kb_backlight.color.extra);
		kb_full_color__set_brightness(kb_backlight.brightness);
		return;
	}

	if (!s76_wmbb(SET_KB_LED, cmds[mode], NULL))
		kb_backlight.mode = mode;
}

static void kb_full_color__set_state(enum kb_state state)
{
	u32 cmd = 0xE0000000;

	S76_DEBUG("State: %d\n", state);

	switch (state) {
	case KB_STATE_OFF:
		cmd |= 0x003001;
		break;
	case KB_STATE_ON:
		cmd |= 0x07F001;
		break;
	default:
		BUG();
	}

	if (!s76_wmbb(SET_KB_LED, cmd, NULL))
		kb_backlight.state = state;
}

static void kb_full_color__init(void)
{
	S76_DEBUG();

	kb_backlight.extra = KB_HAS_EXTRA_FALSE;

	kb_full_color__set_state(param_kb_off ? KB_STATE_OFF : KB_STATE_ON);
	kb_full_color__set_color(param_kb_color[0], param_kb_color[1],
		param_kb_color[2], param_kb_color[3]);
	kb_full_color__set_brightness(param_kb_brightness);
}

static struct kb_backlight_ops kb_full_color_ops = {
	.set_state      = kb_full_color__set_state,
	.set_color      = kb_full_color__set_color,
	.set_brightness = kb_full_color__set_brightness,
	.set_mode       = kb_full_color__set_mode,
	.init           = kb_full_color__init,
};

static void kb_full_color__init_extra(void)
{
	S76_DEBUG();

	kb_backlight.extra = KB_HAS_EXTRA_TRUE;

	kb_full_color__set_state(param_kb_off ? KB_STATE_OFF : KB_STATE_ON);
	kb_full_color__set_color(param_kb_color[0], param_kb_color[1],
		param_kb_color[2], param_kb_color[3]);
	kb_full_color__set_brightness(param_kb_brightness);
}

static struct kb_backlight_ops kb_full_color_with_extra_ops = {
	.set_state      = kb_full_color__set_state,
	.set_color      = kb_full_color__set_color,
	.set_brightness = kb_full_color__set_brightness,
	.set_mode       = kb_full_color__set_mode,
	.init           = kb_full_color__init_extra,
};

static void kb_wmi(u32 event) {
	if (!kb_backlight.ops)
		return;

	switch (event) {
	case 0x81:
		kb_dec_brightness();
		break;
	case 0x82:
		kb_inc_brightness();
		break;
	case 0x83:
		if (!param_kb_cycle_colors)
			kb_next_mode();
		else
			kb_next_color();
		break;
	case 0x9F:
		kb_toggle_state();
		break;
	}
}

/* Sysfs interface */

static ssize_t s76_brightness_show(struct device *child,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", kb_backlight.brightness);
}

static ssize_t s76_brightness_store(struct device *child,
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int val;
	int ret;

	if (!kb_backlight.ops)
		return -EINVAL;

	ret = kstrtouint(buf, 0, &val);
	if (ret)
		return ret;

	kb_backlight.ops->set_brightness(val);

	return ret ? : size;
}

static DEVICE_ATTR(kb_brightness, 0644,
	s76_brightness_show, s76_brightness_store);

static ssize_t s76_state_show(struct device *child,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", kb_backlight.state);
}

static ssize_t s76_state_store(struct device *child,
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int val;
	int ret;

	if (!kb_backlight.ops)
		return -EINVAL;

	ret = kstrtouint(buf, 0, &val);
	if (ret)
		return ret;

	val = clamp_t(unsigned, val, 0, 1);
	kb_backlight.ops->set_state(val);

	return ret ? : size;
}

static DEVICE_ATTR(kb_state, 0644,
	s76_state_show, s76_state_store);

static ssize_t s76_mode_show(struct device *child,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", kb_backlight.mode);
}

static ssize_t s76_mode_store(struct device *child,
	struct device_attribute *attr, const char *buf, size_t size)
{
	static enum kb_mode modes[] = {
		KB_MODE_RANDOM_COLOR,
		KB_MODE_CUSTOM,
		KB_MODE_BREATHE,
		KB_MODE_CYCLE,
		KB_MODE_WAVE,
		KB_MODE_DANCE,
		KB_MODE_TEMPO,
		KB_MODE_FLASH,
	};

	unsigned int val;
	int ret;

	if (!kb_backlight.ops)
		return -EINVAL;

	ret = kstrtouint(buf, 0, &val);
	if (ret)
		return ret;

	val = clamp_t(unsigned, val, 0, 7);
	kb_backlight.ops->set_mode(modes[val]);

	return ret ? : size;
}

static DEVICE_ATTR(kb_mode, 0644,
	s76_mode_show, s76_mode_store);

static ssize_t s76_color_show(struct device *child,
	struct device_attribute *attr, char *buf)
{
	if (kb_backlight.extra == KB_HAS_EXTRA_TRUE)
		return sprintf(buf, "%s %s %s %s\n",
			kb_colors[kb_backlight.color.left].name,
			kb_colors[kb_backlight.color.center].name,
			kb_colors[kb_backlight.color.right].name,
			kb_colors[kb_backlight.color.extra].name);
	else
		return sprintf(buf, "%s %s %s\n",
			kb_colors[kb_backlight.color.left].name,
			kb_colors[kb_backlight.color.center].name,
			kb_colors[kb_backlight.color.right].name);
}

static ssize_t s76_color_store(struct device *child,
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int i, j;
	unsigned int val[4] = {0};
	char left[8];
	char right[8];
	char center[8];
	char extra[8];

	if (!kb_backlight.ops)
		return -EINVAL;

	i = sscanf(buf, "%7s %7s %7s %7s", left, center, right, extra);

	if (i == 1) {
		for (j = 0; j < ARRAY_SIZE(kb_colors); j++) {
			if (!strcmp(left, kb_colors[j].name))
				val[0] = j;
		}
		val[0] = clamp_t(unsigned, val[0], 0, ARRAY_SIZE(kb_colors));
		val[3] = val[2] = val[1] = val[0];

	} else if (i == 3 || i == 4) {
		for (j = 0; j < ARRAY_SIZE(kb_colors); j++) {
			if (!strcmp(left, kb_colors[j].name))
				val[0] = j;
			if (!strcmp(center, kb_colors[j].name))
				val[1] = j;
			if (!strcmp(right, kb_colors[j].name))
				val[2] = j;
			if (!strcmp(extra, kb_colors[j].name))
				val[3] = j;
		}
		val[0] = clamp_t(unsigned, val[0], 0, ARRAY_SIZE(kb_colors));
		val[1] = clamp_t(unsigned, val[1], 0, ARRAY_SIZE(kb_colors));
		val[2] = clamp_t(unsigned, val[2], 0, ARRAY_SIZE(kb_colors));
		val[3] = clamp_t(unsigned, val[3], 0, ARRAY_SIZE(kb_colors));

	} else
		return -EINVAL;

	kb_backlight.ops->set_color(val[0], val[1], val[2], val[3]);

	return size;
}
static DEVICE_ATTR(kb_color, 0644,
	s76_color_show, s76_color_store);
