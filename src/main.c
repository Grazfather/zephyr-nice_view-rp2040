#include <stdlib.h>

#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/display.h>

#if DT_NODE_HAS_PROP(DT_ALIAS(led_strip), chain_length)
#define STRIP_NUM_PIXELS DT_PROP(DT_ALIAS(led_strip), chain_length)
#else
#error Unable to determine length of LED strip
#endif

#define DELAY_TIME K_MSEC(200)

#define RGB(_r, _g, _b) { .r = (_r), .g = (_g), .b = (_b) }

static const struct led_rgb colors[] = {
	RGB(0x0f, 0x00, 0x00),
	RGB(0x00, 0x0f, 0x00),
	RGB(0x00, 0x00, 0x0f),
};

static struct led_rgb pixels[STRIP_NUM_PIXELS];
static const struct device *const strip = DEVICE_DT_GET(DT_ALIAS(led_strip));

static const struct device *const display = DEVICE_DT_GET(DT_ALIAS(nice_view));

enum corner {
	TOP_LEFT,
	TOP_RIGHT,
	BOTTOM_RIGHT,
	BOTTOM_LEFT
};

typedef void (*fill_buffer)(enum corner corner, uint8_t grey, uint8_t *buf,
			    size_t buf_size);

static void fill_buffer_mono(enum corner corner, uint8_t grey,
			     uint8_t black, uint8_t white,
			     uint8_t *buf, size_t buf_size)
{
	uint16_t color;

	switch (corner) {
	case BOTTOM_LEFT:
		color = (grey & 0x01u) ? white : black;
		break;
	default:
		color = black;
		break;
	}

	memset(buf, color, buf_size);
}

static inline void fill_buffer_mono01(enum corner corner, uint8_t grey,
				      uint8_t *buf, size_t buf_size)
{
	fill_buffer_mono(corner, grey, 0x00u, 0xFFu, buf, buf_size);
}

static void led_thread_fn(void *a, void *b, void *c)
{
	size_t color = 0;
	int rc;
	while (1) {
		for (size_t cursor = 0; cursor < ARRAY_SIZE(pixels); cursor++) {
			memset(&pixels, 0, sizeof(pixels));
			memcpy(&pixels[cursor], &colors[color], sizeof(struct led_rgb));

			rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
			if (rc) {
				LOG_ERR("couldn't update strip: %d", rc);
			}

			k_sleep(DELAY_TIME);
		}
		color = (color + 1) & ARRAY_SIZE(colors);
	}
}
K_THREAD_STACK_DEFINE(led_thread_stack, 256);
struct k_thread led_thread;

int main(void)
{
	size_t x;
	size_t y;
	size_t h_step;
	size_t grey_count;
	uint8_t bg_color;
	uint8_t *buf;
	int32_t grey_scale_sleep;
	struct display_capabilities capabilities;
	struct display_buffer_descriptor buf_desc;
	size_t buf_size = 0;
	fill_buffer fill_buffer_fnc = NULL;

	if (device_is_ready(strip)) {
		LOG_INF("Found LED device %s", strip->name);
	} else {
		LOG_ERR("LED device %s is not ready", strip->name);
		return 0;
	}

	k_tid_t led_tid = k_thread_create(&led_thread,
			led_thread_stack,
			K_THREAD_STACK_SIZEOF(led_thread_stack),
			led_thread_fn,
			NULL, NULL, NULL,
			3, 0, K_NO_WAIT);

	if (device_is_ready(display)) {
		LOG_INF("Found display device %s", display->name);
	} else {
		LOG_ERR("display device %s is not ready", display->name);
		return 0;
	}

	display_get_capabilities(display, &capabilities);

	if (capabilities.screen_info & SCREEN_INFO_EPD) {
		grey_scale_sleep = 10000;
	} else {
		grey_scale_sleep = 100;
	}

	buf_size = (capabilities.x_resolution * capabilities.y_resolution) / 8;

	bg_color = 0xFFu;
	fill_buffer_fnc = fill_buffer_mono01;

	buf = malloc(buf_size);

	if (buf == NULL) {
		LOG_ERR("Could not allocate memory. Aborting app.");
		return 0;
	}

	(void)memset(buf, bg_color, buf_size);

	buf_desc.buf_size = buf_size;
	buf_desc.height = 8;
	buf_desc.pitch = capabilities.x_resolution;
	buf_desc.width = capabilities.x_resolution;

	fill_buffer_fnc(TOP_LEFT, 0, buf, buf_size);
	x = 0;
	y = 0;
	display_write(display, x, y, &buf_desc, buf);

	display_blanking_off(display);

	grey_count = 0;
	x = 0;
	y = 0;

	while (1) {
		fill_buffer_fnc(BOTTOM_LEFT, grey_count, buf, buf_size);
		display_write(display, x, y, &buf_desc, buf);
		++grey_count;
		k_msleep(grey_scale_sleep);
		buf_desc.height += 8;
		buf_desc.height = buf_desc.height % 68;
	}

	return 0;
}
