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
	lv_disp_t *disp = lv_disp_get_default();
	// lv_disp_set_rotation(disp, LV_DISP_ROT_180); // Works!
	// lv_disp_set_rotation(disp, LV_DISP_ROT_270);
	lv_obj_t *screen = lv_scr_act();

	lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
	lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

	lv_obj_t *label = lv_label_create(screen);
	lv_obj_set_style_text_color(label, lv_color_black(), 0);
	lv_label_set_text(label, "Hello, World!");
	lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
	lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

	lv_task_handler();
	printk("Label displayed\n");

	while (1)
	{
		k_sleep(K_FOREVER);
	}
}
