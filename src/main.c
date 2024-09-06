
#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/device.h>

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

int main(void)
{
	size_t color = 0;
	int rc;

	if (device_is_ready(strip)) {
		LOG_INF("Found LED device %s", strip->name);
	} else {
		LOG_ERR("LED device %s is not ready", strip->name);
		return 0;
	}

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

	return 0;
}
