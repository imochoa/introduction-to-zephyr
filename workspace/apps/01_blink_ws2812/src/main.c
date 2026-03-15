#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/driversa/spi.h>

// Settings
static const int32_t sleep_time_ms = 1000;

#define STRIP_NODE DT_ALIAS(led_strip)
#define STRIP_NUM_PIXELS DT_PROP(DT_ALIAS(led_strip), chain_length)

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);
static struct led_rgb pixels[STRIP_NUM_PIXELS];

int main(void)
{
	int ret;
	int state = 0;

	// Make sure that the LED strip device was initialized
	if (!device_is_ready(strip))
	{
		return 0;
	}

	// Do forever
	while (1)
	{

		// Change the state and print
		state = !state;
		printk("LED state: %d\r\n", state);

		// Set the LED color (0-255 or 0x00-0xFF)
		if (state)
		{
			pixels[0].r = 0x20;
			pixels[0].g = 0x00;
			pixels[0].b = 0x00;
		}
		else
		{
			pixels[0].r = 0x00;
			pixels[0].g = 0x00;
			pixels[0].b = 0x00;
		}

		// Update the strip
		ret = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
		if (ret < 0)
		{
			return 0;
		}

		// Sleep
		k_msleep(sleep_time_ms);
	}

	return 0;
}
