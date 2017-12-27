/*
 * fan.c
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

#if S76_HAS_HWMON
struct s76_hwmon {
	struct device *dev;
};

static struct s76_hwmon *s76_hwmon = NULL;

static int
s76_read_fan(int idx)
{
	u8 value;
	int raw_rpm;
	ec_read(0xd0 + 0x2 * idx, &value);
	raw_rpm = value << 8;
	ec_read(0xd1 + 0x2 * idx, &value);
	raw_rpm += value;
	if (!raw_rpm)
		return 0;
	return 2156220 / raw_rpm;
}

static ssize_t
s76_hwmon_show_name(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	return sprintf(buf, S76_DRIVER_NAME "\n");
}

static ssize_t
s76_hwmon_show_fan1_input(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%i\n", s76_read_fan(0));
}

static ssize_t
s76_hwmon_show_fan1_label(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "CPU fan\n");
}

#ifdef EXPERIMENTAL
static ssize_t
s76_hwmon_show_fan2_input(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%i\n", s76_read_fan(1));
}

static ssize_t
s76_hwmon_show_fan2_label(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "GPU fan\n");
}
#endif

static ssize_t
s76_hwmon_show_temp1_input(struct device *dev, struct device_attribute *attr,
				 char *buf)
{
	u8 value;
	ec_read(0x07, &value);
	return sprintf(buf, "%i\n", value * 1000);
}

static ssize_t
s76_hwmon_show_temp1_label(struct device *dev, struct device_attribute *attr,
				 char *buf)
{
	return sprintf(buf, "CPU temperature\n");
}

#ifdef EXPERIMENTAL
static ssize_t
s76_hwmon_show_temp2_input(struct device *dev, struct device_attribute *attr,
				 char *buf)
{
	u8 value;
	ec_read(0xcd, &value);
	return sprintf(buf, "%i\n", value * 1000);
}

static ssize_t
s76_hwmon_show_temp2_label(struct device *dev, struct device_attribute *attr,
				 char *buf)
{
	return sprintf(buf, "GPU temperature\n");
}
#endif

static SENSOR_DEVICE_ATTR(name, S_IRUGO, s76_hwmon_show_name, NULL, 0);
static SENSOR_DEVICE_ATTR(fan1_input, S_IRUGO, s76_hwmon_show_fan1_input, NULL, 0);
static SENSOR_DEVICE_ATTR(fan1_label, S_IRUGO, s76_hwmon_show_fan1_label, NULL, 0);
#ifdef EXPERIMENTAL
static SENSOR_DEVICE_ATTR(fan2_input, S_IRUGO, s76_hwmon_show_fan2_input, NULL, 0);
static SENSOR_DEVICE_ATTR(fan2_label, S_IRUGO, s76_hwmon_show_fan2_label, NULL, 0);
#endif
static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO, s76_hwmon_show_temp1_input, NULL, 0);
static SENSOR_DEVICE_ATTR(temp1_label, S_IRUGO, s76_hwmon_show_temp1_label, NULL, 0);
#ifdef EXPERIMENTAL
static SENSOR_DEVICE_ATTR(temp2_input, S_IRUGO, s76_hwmon_show_temp2_input, NULL, 0);
static SENSOR_DEVICE_ATTR(temp2_label, S_IRUGO, s76_hwmon_show_temp2_label, NULL, 0);
#endif

static struct attribute *hwmon_default_attributes[] = {
	&sensor_dev_attr_name.dev_attr.attr,
	&sensor_dev_attr_fan1_input.dev_attr.attr,
	&sensor_dev_attr_fan1_label.dev_attr.attr,
#ifdef EXPERIMENTAL
	&sensor_dev_attr_fan2_input.dev_attr.attr,
	&sensor_dev_attr_fan2_label.dev_attr.attr,
#endif
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_label.dev_attr.attr,
#ifdef EXPERIMENTAL
	&sensor_dev_attr_temp2_input.dev_attr.attr,
	&sensor_dev_attr_temp2_label.dev_attr.attr,
#endif
	NULL
};

static const struct attribute_group hwmon_default_attrgroup = {
	.attrs = hwmon_default_attributes,
};

static int
s76_hwmon_init(struct device *dev)
{
	int ret;

	s76_hwmon = kzalloc(sizeof(*s76_hwmon), GFP_KERNEL);
	if (!s76_hwmon)
		return -ENOMEM;
	s76_hwmon->dev = hwmon_device_register(dev);
	if (IS_ERR(s76_hwmon->dev)) {
		ret = PTR_ERR(s76_hwmon->dev);
		s76_hwmon->dev = NULL;
		return ret;
	}

	ret = sysfs_create_group(&s76_hwmon->dev->kobj, &hwmon_default_attrgroup);
	if (ret)
		return ret;
	return 0;
}

static int
s76_hwmon_fini(struct device *dev)
{
	if (!s76_hwmon || !s76_hwmon->dev)
		return 0;
	sysfs_remove_group(&s76_hwmon->dev->kobj, &hwmon_default_attrgroup);
	hwmon_device_unregister(s76_hwmon->dev);
	kfree(s76_hwmon);
	return 0;
}
#endif // S76_HAS_HWMON