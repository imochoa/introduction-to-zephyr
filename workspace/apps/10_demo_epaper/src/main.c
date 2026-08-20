#include <lvgl.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>

#include "demos.h"

static void show_demo(enum epaper_demo_id id, uint32_t sequence)
{
	printk("Rendering LVGL demo: %s\n", epaper_demo_name(id));
	epaper_demo_render(id, sequence);

	/* Render now. The final LVGL tile triggers the e-paper refresh. */
	lv_refr_now(NULL);
	printk("Refresh complete: %s\n", epaper_demo_name(id));
}

int main(void)
{
	const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	struct display_capabilities capabilities;
	uint32_t sequence = 0;

	printk("\n=== E-Paper LVGL demos ===\n");

	if (!device_is_ready(display)) {
		printk("Display device not ready\n");
		return 0;
	}

	display_get_capabilities(display, &capabilities);
	printk("Display ready: %u x %u\n", capabilities.x_resolution, capabilities.y_resolution);

#if defined(CONFIG_EPAPER_DEMO_CYCLE)
	for (;;) {
		enum epaper_demo_id id = sequence % EPAPER_DEMO_ID_COUNT;

		show_demo(id, sequence);
		sequence++;
		k_sleep(K_SECONDS(CONFIG_EPAPER_DEMO_INTERVAL_SECONDS));
	}
#else
#if defined(CONFIG_EPAPER_DEMO_STATUS)
	show_demo(EPAPER_DEMO_ID_STATUS, sequence);
#elif defined(CONFIG_EPAPER_DEMO_PATTERN)
	show_demo(EPAPER_DEMO_ID_PATTERN, sequence);
#elif defined(CONFIG_EPAPER_DEMO_SIDEWAYS)
	show_demo(EPAPER_DEMO_ID_SIDEWAYS, sequence);
#else
	show_demo(EPAPER_DEMO_ID_HELLO, sequence);
#endif

	for (;;) {
		k_sleep(K_FOREVER);
	}
#endif

	return 0;
}
