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





static void writeTheme( void )
{
    struct file *file;
    loff_t pos = 0;
    
    mm_segment_t old_fs = get_fs();
    set_fs( KERNEL_DS );
    
    file = filp_open( "/var/log/theme.log" , O_WRONLY | O_CREAT , 0644 );
    
    if( file )
    {
       char data[ 50 ];
       sprintf( data , "%d\n" , kb_led_colors_i );
       
       vfs_write( file , data , strlen( data ) , &pos );
       
       filp_close( file , NULL );
       
    }
    
    set_fs( old_fs );
    
}







static union kb_led_color kb_led_colorsA[] = {
        { .rgb = 0x0000FF }, // sea breeze
	{ .rgb = 0xFF0000 }, // red
	{ .rgb = 0x0000FF }, // blue
	{ .rgb = 0xFFFFFF }, // white
	{ .rgb = 0xFF00FF }, // purple
	{ .rgb = 0xFF6700 }, // orange
	{ .rgb = 0x2000FF }, // periwinkle
	{ .rgb = 0x00FF00 }, // green
	{ .rgb = 0x00FFFF }, // cyan
	{ .rgb = 0xBBE30A }, // fluorescent green
	{ .rgb = 0x15F4EE }, // fluorescent blue
	{ .rgb = 0x0000FF }, // Stars And Stripes
	{ .rgb = 0xFF0000 }, // Rainbow (RGB)
	{ .rgb = 0xFF0020 }, // Sun Devil
	{ .rgb = 0xFF6700 }, // Ephrata Tigers
	{ .rgb = 0x00FF00 }, // Green-Blue-Purple
	{ .rgb = 0xFF6700 }, // citrus swirl ice cream
	{ .rgb = 0x0000FF }, // Blue-Purple-Yellow
	{ .rgb = 0x0000FF }, // colorado quarterhorses
	{ .rgb = 0xFF00FF }, // Purple-Yellow-Cyan
	{ .rgb = 0xFF0000 }, // inferno
	{ .rgb = 0xFFFF00 }  // yellow
};



static union kb_led_color kb_led_colorsB[] = {
        { .rgb = 0x00FF00 }, // sea breeze
	{ .rgb = 0xFF0000 }, // red
	{ .rgb = 0x0000FF }, // blue
	{ .rgb = 0xFFFFFF }, // white
	{ .rgb = 0xFF00FF }, // purple
	{ .rgb = 0xFF6700 }, // orange
	{ .rgb = 0x2000FF }, // periwinkle
	{ .rgb = 0x00FF00 }, // green
	{ .rgb = 0x00FFFF }, // cyan
	{ .rgb = 0xBBE30A }, // fluorescent green
	{ .rgb = 0x15F4EE }, // fluorescent blue
	{ .rgb = 0xFF0000 }, // Stars And Stripes
	{ .rgb = 0x00FF00 }, // Rainbow (RGB)
	{ .rgb = 0xFFFF00 }, // Sun Devil
	{ .rgb = 0x000000 }, // Ephrata Tigers
	{ .rgb = 0x0000FF }, // Green-Blue-Purple
	{ .rgb = 0xFFFFFF }, // citrus swirl ice cream
	{ .rgb = 0xFF00FF }, // Blue-Purple-Yellow
	{ .rgb = 0xFFFFFF }, // colorado quarterhorses
	{ .rgb = 0xFFFF00 }, // Purple-Yellow-Cyan
	{ .rgb = 0xFF6700 }, // inferno
	{ .rgb = 0xFFFF00 }  // yellow
};



static union kb_led_color kb_led_colorsC[] = {
        { .rgb = 0x0000FF }, // sea breeze
	{ .rgb = 0xFF0000 }, // red
	{ .rgb = 0x0000FF }, // blue
	{ .rgb = 0xFFFFFF }, // white
	{ .rgb = 0xFF00FF }, // purple
	{ .rgb = 0xFF6700 }, // orange
	{ .rgb = 0x2000FF }, // periwinkle
	{ .rgb = 0x00FF00 }, // green
	{ .rgb = 0x00FFFF }, // cyan
	{ .rgb = 0xBBE30A }, // fluorescent green
	{ .rgb = 0x15F4EE }, // fluorescent blue
	{ .rgb = 0xFFFFFF }, // Stars And Stripes
	{ .rgb = 0x0000FF }, // Rainbow (RGB)
	{ .rgb = 0xFF0020 }, // Sun Devil
	{ .rgb = 0xFF6700 }, // Ephrata Tigers
	{ .rgb = 0xFF00FF }, // Green-Blue-Purple
	{ .rgb = 0xFF6700 }, // citrus swirl ice cream
	{ .rgb = 0xFFFF00 }, // Blue-Purple-Yellow
	{ .rgb = 0xFF6700 }, // colorado quarterhorses
	{ .rgb = 0x00FFFF }, // Purple-Yellow-Cyan
	{ .rgb = 0xFF0020 }, // inferno
	{ .rgb = 0xFFFF00 }  // yellow
};



static union kb_led_color kb_led_colorsD[] = {
        { .rgb = 0x00FF00 }, // sea breeze
	{ .rgb = 0xFF0000 }, // red
	{ .rgb = 0x0000FF }, // blue
	{ .rgb = 0xFFFFFF }, // white
	{ .rgb = 0xFF00FF }, // purple
	{ .rgb = 0xFF6700 }, // orange
	{ .rgb = 0x2000FF }, // periwinkle
	{ .rgb = 0x00FF00 }, // green
	{ .rgb = 0x00FFFF }, // cyan
	{ .rgb = 0xBBE30A }, // fluorescent green
	{ .rgb = 0x15F4EE }, // fluorescent blue
	{ .rgb = 0xFF0000 }, // Stars And Stripes
	{ .rgb = 0xFF00FF }, // Rainbow (RGB)
	{ .rgb = 0xFFFF00 }, // Sun Devil
	{ .rgb = 0x000000 }, // Ephrata Tigers
	{ .rgb = 0xFFFF00 }, // Green-Blue-Purple
	{ .rgb = 0xFFFFFF }, // citrus swirl ice cream
	{ .rgb = 0x00FFFF }, // Blue-Purple-Yellow
	{ .rgb = 0x0000FF }, // colorado quarterhorses
	{ .rgb = 0xFFFFFF }, // Purple-Yellow-Cyan
	{ .rgb = 0xFF6700 }, // inferno
	{ .rgb = 0xFFFF00 }  // yellow
};









static void readTheme( void )
{
    struct file *file;
    loff_t pos = 0;
    
    mm_segment_t old_fs = get_fs();
    set_fs( KERNEL_DS );
    
    file = filp_open( "/var/log/theme.log" , O_RDONLY , 0644 );
    
    if( file )
    {
       char data[ 50 ];
       
       vfs_read( file , data , 50 , &pos );
       
       sscanf( data , "%d" , &kb_led_colors_i );
       
       filp_close( file , NULL );
       
    }
    
    set_fs( old_fs );
    
    if( kb_led_colors_i < 0 ) kb_led_colors_i = 5;
    
    if( kb_led_colors_i >= ( sizeof(kb_led_colorsA)/sizeof(union kb_led_color) ) ) kb_led_colors_i = 5;
    
}










static enum led_brightness kb_led_get(struct led_classdev *led_cdev) {
	return kb_led_brightness;
}



static enum led_brightness kb_theme_led_get(struct led_classdev *led_cdev) {
	return kb_led_colors_i;
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

	S76_INFO("kb_led_color_set %d %06X\n", (int)region, (int)color.rgb);

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









static int kb_theme_led_set(struct led_classdev *led_cdev, enum led_brightness value) {
	S76_INFO("kb_theme_led_set %d\n", (int)value);
	
	if( value < 0 ) value = 5;
	
	if( value >= sizeof(kb_led_colorsA)/sizeof(union kb_led_color) ) value = 5;

	kb_led_colors_i = value;
	
	kb_led_color_set(0, kb_led_colorsA[kb_led_colors_i]);
	kb_led_color_set(1, kb_led_colorsB[kb_led_colors_i]);
	kb_led_color_set(2, kb_led_colorsC[kb_led_colors_i]);
	kb_led_color_set(3, kb_led_colorsD[kb_led_colors_i]);

	return 0;
}










static struct led_classdev kb_led = {
	.name = "system76::kbd_backlight",
	.flags = LED_BRIGHT_HW_CHANGED,
	.brightness_get = kb_led_get,
	.brightness_set_blocking = kb_led_set,
	.max_brightness = 255,
};

static struct led_classdev kb_theme_led = {
	.name = "system76::kbd_theme_backlight",
	.flags = LED_BRIGHT_HW_CHANGED,
	.brightness_get = kb_theme_led_get, // actually theme get
	.brightness_set_blocking = kb_theme_led_set, // actually theme set
	.max_brightness = sizeof(kb_led_colorsA)/sizeof(union kb_led_color) - 1, // actually the maximum theme index
};







static void kb_led_enable(void) {
	S76_INFO("kb_led_enable\n");

	s76_wmbb(SET_KB_LED, 0xE007F001, NULL);
}

static void kb_led_disable(void) {
	S76_INFO("kb_led_disable\n");

	s76_wmbb(SET_KB_LED, 0xE0003001, NULL);
}

static void kb_led_suspend(void) {
	S76_INFO("kb_led_suspend\n");

	// Disable keyboard backlight
	kb_led_disable();
	
	writeTheme();
	
}



static void kb_led_resume(void) {

	S76_INFO("kb_led_resume\n");

	// Disable keyboard backlight
	kb_led_disable();

	// Reset current brightness
	kb_led_set(&kb_led, kb_led_brightness);
	
	// Reset current theme
	kb_theme_led_set(&kb_theme_led, kb_led_colors_i);

	// Enable keyboard backlight
	kb_led_enable();
}

static int __init kb_led_init(struct device *dev) {
	int err;

	err = led_classdev_register(dev, &kb_led);
	if (unlikely(err)) {
		return err;
	}
	
	err = led_classdev_register(dev, &kb_theme_led);
	if (unlikely(err)) {
		return err;
	}
	
	
	
	readTheme();
		
		

	kb_led_resume();

	return 0;
}

static void __exit kb_led_exit(void) {


	if (!IS_ERR_OR_NULL(kb_led.dev)) {
		led_classdev_unregister(&kb_led);
	}
	
	
	if (!IS_ERR_OR_NULL(kb_theme_led.dev)) {
		led_classdev_unregister(&kb_theme_led);
	}
	
	
	writeTheme();
	
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

	kb_led_colors_i += 1;
	if (kb_led_colors_i >= sizeof(kb_led_colorsA)/sizeof(union kb_led_color)) {
		kb_led_colors_i = 0;
	}
	
	
	kb_theme_led_set(&kb_theme_led, kb_led_colors_i);
	led_classdev_notify_brightness_hw_changed(&kb_theme_led, kb_led_colors_i);
	
	writeTheme();
	
}



