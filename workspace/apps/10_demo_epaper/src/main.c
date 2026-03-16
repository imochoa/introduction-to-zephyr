#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <string.h>

#define DISPLAY_NODE DT_CHOSEN(zephyr_display)

int main(void)
{
	const struct device *display = DEVICE_DT_GET(DISPLAY_NODE);
	struct display_capabilities caps;
	struct display_buffer_descriptor desc;
	size_t buf_size;
	uint8_t *buf;

	printk("\n\n=== E-Paper Demo Start ===\n");

	if (!device_is_ready(display)) {
		printk("Display device not ready\n");
		return 0;
	}

	display_get_capabilities(display, &caps);
	printk("Display: %ux%u, pixel format %u\n",
	       caps.x_resolution, caps.y_resolution,
	       caps.current_pixel_format);

	buf_size = (caps.x_resolution * caps.y_resolution) / 8;
	buf = k_malloc(buf_size);
	if (!buf) {
		printk("Failed to allocate framebuffer (%u bytes)\n",
		       (unsigned int)buf_size);
		return 0;
	}

	desc.width = caps.x_resolution;
	desc.height = caps.y_resolution;
	desc.pitch = caps.x_resolution;
	desc.buf_size = buf_size;

	display_blanking_on(display);

	memset(buf, 0xFF, buf_size);
	display_write(display, 0, 0, &desc, buf);
	display_blanking_off(display);
	printk("Cleared to white\n");

	k_sleep(K_SECONDS(3));

	display_blanking_on(display);

	memset(buf, 0x00, buf_size);
	display_write(display, 0, 0, &desc, buf);
	display_blanking_off(display);
	printk("Filled black\n");

	k_sleep(K_SECONDS(3));

	display_blanking_on(display);

	memset(buf, 0xFF, buf_size);

	uint16_t stride = caps.x_resolution / 8;

	for (uint16_t y = 0; y < caps.y_resolution; y++) {
		for (uint16_t x_byte = 0; x_byte < stride; x_byte++) {
			if ((y / 16 + x_byte / 4) % 2 == 0) {
				buf[y * stride + x_byte] = 0x00;
			}
		}
	}

	display_write(display, 0, 0, &desc, buf);
	display_blanking_off(display);
	printk("Checkerboard pattern drawn\n");

	k_free(buf);

	while (1) {
		k_sleep(K_FOREVER);
	}
}
