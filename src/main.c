
#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>

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

	while (1) {
		k_sleep(K_MSEC(20000));
	}

	return 0;
}
