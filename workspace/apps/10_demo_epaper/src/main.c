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
	// disp->driver->sw_rotate = 1;
	// lv_disp_set_rotation(disp, LV_DISP_ROT_90);
	lv_obj_t *screen = lv_scr_act();

	// lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
	// lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

	lv_obj_t *label = lv_label_create(screen);
	// lv_obj_set_style_text_color(label, lv_color_black(), 0);
	lv_label_set_text(label, "Hello, World!");
	lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
	lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

	// Redraws screen
	lv_task_handler();
	/*
	 * CONFIG_LV_Z_FULL_REFRESH sends one flush where is_last=true.
	 * Zephyr's lvgl_flush_cb_mono only calls display_blanking_off()
	 * when blanking_on was previously set (via !is_last), which never
	 * happens with a single flush. Trigger the EPD refresh manually.
	 */
	display_blanking_off(display);
	printk("Label displayed\n");

	// show text
	// https://www.youtube.com/redirect?event=video_description&redir_token=QUFFLUhqbXdpY1BoNXhZUkFLZk1LWTJsRFNVYS14QjRNQXxBQ3Jtc0trVk43LUg2S2ZCTlJNckJRbER1NjJyZFNRelMzREJDSVlpM25hbDlCcGQ4MW0zTW1La1RGd00xN3cwZ282SVVrU1prQTlrWmtxWlZtUnNxREpXal9XNDRaU0NXSE9tZ2dlWmlSbTI5YXVEaGZxam5QNA&q=https%3A%2F%2Fgithub.com%2Fcircuitdojo%2Fair-quality-wing-zephyr-demo%2Ftree%2Fmain%2Fbasic_with_display&v=fRB9gn77XhE
	// snprintf(temp_value, sizeof(temp_value) - 1,
	// "%d.%d", data->val.vall,
	// get two_digits(data->val.val2));
	// Iv _label_set_text(temp_value_label,
	// temp_value)

	while (1)
	{
		k_sleep(K_FOREVER);
	}
}
