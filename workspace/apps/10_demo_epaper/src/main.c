#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>

int main(void)
{
	const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	printk("\n\n=== E-Paper Demo Start ===\n");

	if (!device_is_ready(display))
	{
		printk("Display device not ready\n");
		return 0;
	}

	lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);
	lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

	lv_obj_t *label = lv_label_create(lv_scr_act());
	lv_obj_set_style_text_color(label, lv_color_black(), 0);
	lv_label_set_text(label, "Hello, World!");
	lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

	lv_task_handler();
	printk("Label displayed\n");

	while (1)
	{
		k_sleep(K_FOREVER);
	}
}
