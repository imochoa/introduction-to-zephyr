#include "demos.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font)
{
	lv_obj_t *label = lv_label_create(parent);

	lv_label_set_text(label, text);
	lv_obj_set_style_text_color(label, lv_color_black(), 0);
	lv_obj_set_style_text_font(label, font, 0);
	lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

	return label;
}

static lv_obj_t *make_box(lv_obj_t *parent, lv_coord_t width, lv_coord_t height, bool filled)
{
	lv_obj_t *box = lv_obj_create(parent);

	lv_obj_remove_style_all(box);
	lv_obj_set_size(box, width, height);
	lv_obj_set_style_bg_color(box, filled ? lv_color_black() : lv_color_white(), 0);
	lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(box, lv_color_black(), 0);
	lv_obj_set_style_border_width(box, 2, 0);
	lv_obj_set_style_pad_all(box, 8, 0);
	lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

	return box;
}

static lv_obj_t *prepare_screen(void)
{
	lv_obj_t *screen = lv_scr_act();

	lv_obj_clean(screen);
	lv_obj_remove_style_all(screen);
	lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
	lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
	lv_obj_set_style_text_color(screen, lv_color_black(), 0);
	lv_obj_set_style_pad_all(screen, 0, 0);
	lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

	return screen;
}

#define CARD_IMAGE_WIDTH 64
#define CARD_IMAGE_HEIGHT 190
#define FREE_IMAGE_WIDTH 32
#define FREE_IMAGE_HEIGHT 90

static uint8_t card_image_buffer[LV_IMG_BUF_SIZE_INDEXED_1BIT(CARD_IMAGE_WIDTH,
							      CARD_IMAGE_HEIGHT)];
static uint8_t free_image_buffer[LV_IMG_BUF_SIZE_INDEXED_1BIT(FREE_IMAGE_WIDTH,
							      FREE_IMAGE_HEIGHT)];
static lv_img_dsc_t card_image_dsc;
static lv_img_dsc_t free_image_dsc;

static void set_image_pixel(uint8_t *buffer, lv_coord_t width, lv_coord_t x, lv_coord_t y)
{
	uint8_t *pixels = buffer + (sizeof(lv_color32_t) * 2U);
	uint32_t stride = ((uint32_t)width + 7U) / 8U;

	pixels[(uint32_t)y * stride + ((uint32_t)x / 8U)] |=
		1U << (7U - ((uint32_t)x & 7U));
}

static lv_obj_t *make_rotated_text_image(lv_obj_t *parent, const char *text,
					 const lv_font_t *font, lv_coord_t width,
					 lv_coord_t height, bool clockwise,
					 lv_color_t foreground, lv_color_t background,
					 uint8_t *buffer, size_t buffer_size,
					 lv_img_dsc_t *image_dsc)
{
	lv_coord_t source_width = height;
	lv_coord_t source_height = width;
	lv_coord_t text_width = 0;
	lv_coord_t pen_x;
	lv_coord_t line_y;

	for (size_t i = 0; text[i] != '\0'; i++) {
		text_width += lv_font_get_glyph_width(font, (uint8_t)text[i],
						      (uint8_t)text[i + 1]);
	}

	memset(buffer, 0, buffer_size);
	memset(image_dsc, 0, sizeof(*image_dsc));
	image_dsc->header.cf = LV_IMG_CF_INDEXED_1BIT;
	image_dsc->header.w = width;
	image_dsc->header.h = height;
	image_dsc->data_size = buffer_size;
	image_dsc->data = buffer;
	lv_img_buf_set_palette(image_dsc, 0, background);
	lv_img_buf_set_palette(image_dsc, 1, foreground);

	pen_x = (source_width - text_width) / 2;
	line_y = (source_height - font->line_height) / 2;

	for (size_t i = 0; text[i] != '\0'; i++) {
		uint32_t letter = (uint8_t)text[i];
		uint32_t next = (uint8_t)text[i + 1];
		lv_font_glyph_dsc_t glyph;
		const uint8_t *bitmap;
		lv_coord_t glyph_x;
		lv_coord_t glyph_y;

		if (!lv_font_get_glyph_dsc(font, &glyph, letter, next)) {
			continue;
		}

		bitmap = lv_font_get_glyph_bitmap(glyph.resolved_font, letter);
		glyph_x = pen_x + glyph.ofs_x;
		glyph_y = line_y + font->line_height - font->base_line - glyph.box_h - glyph.ofs_y;

		if (bitmap != NULL) {
			uint8_t mask = (1U << glyph.bpp) - 1U;

			for (lv_coord_t y = 0; y < glyph.box_h; y++) {
				for (lv_coord_t x = 0; x < glyph.box_w; x++) {
					uint32_t bit_index =
						((uint32_t)y * glyph.box_w + x) * glyph.bpp;
					uint8_t value =
						(bitmap[bit_index / 8U] >>
						 (8U - glyph.bpp - (bit_index & 7U))) &
						mask;
					lv_coord_t source_x = glyph_x + x;
					lv_coord_t source_y = glyph_y + y;
					lv_coord_t dest_x;
					lv_coord_t dest_y;

					if (value < ((mask + 1U) / 2U) || source_x < 0 ||
					    source_x >= source_width || source_y < 0 ||
					    source_y >= source_height) {
						continue;
					}

					if (clockwise) {
						dest_x = source_height - 1 - source_y;
						dest_y = source_x;
					} else {
						dest_x = source_y;
						dest_y = source_width - 1 - source_x;
					}

					set_image_pixel(buffer, width, dest_x, dest_y);
				}
			}
		}

		pen_x += glyph.adv_w;
	}

	lv_obj_t *image = lv_img_create(parent);
	lv_img_set_src(image, image_dsc);
	return image;
}

static void render_hello(lv_obj_t *screen)
{
	lv_obj_t *title = make_label(screen, "LVGL + E-PAPER", &lv_font_montserrat_20);
	lv_obj_t *card = make_box(screen, 200, 112, true);
	lv_obj_t *message =
		make_label(card, "Hello!\n\nThe display works.", &lv_font_montserrat_14);
	lv_obj_t *footer = make_label(screen, "ESP32-S3 / UC8253", &lv_font_montserrat_14);

	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 38);
	lv_obj_align(card, LV_ALIGN_CENTER, 0, -5);
	lv_obj_set_style_text_color(message, lv_color_white(), 0);
	lv_obj_center(message);
	lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -38);
}

static void render_status(lv_obj_t *screen, uint32_t sequence)
{
	char status[128];
	uint32_t percentage = 25U + ((sequence * 25U) % 76U);
	lv_obj_t *title = make_label(screen, "DISPLAY STATUS", &lv_font_montserrat_20);
	lv_obj_t *divider = make_box(screen, 200, 3, true);
	lv_obj_t *details;
	lv_obj_t *bar_outline;
	lv_obj_t *bar_fill;
	lv_obj_t *footer;

	snprintf(status, sizeof(status),
		 "Panel: 240 x 416\nController: UC8253\nRefresh: %u\nUptime: %lld s", sequence + 1U,
		 k_uptime_get() / 1000);
	details = make_label(screen, status, &lv_font_montserrat_14);

	bar_outline = make_box(screen, 190, 28, false);
	lv_obj_set_style_pad_all(bar_outline, 3, 0);
	bar_fill = make_box(bar_outline, (180 * percentage) / 100, 18, true);
	lv_obj_set_style_border_width(bar_fill, 0, 0);
	lv_obj_align(bar_fill, LV_ALIGN_LEFT_MID, 0, 0);

	snprintf(status, sizeof(status), "DEMO LEVEL %u%%", percentage);
	footer = make_label(screen, status, &lv_font_montserrat_14);

	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 32);
	lv_obj_align(divider, LV_ALIGN_TOP_MID, 0, 68);
	lv_obj_align(details, LV_ALIGN_TOP_MID, 0, 100);
	lv_obj_align(bar_outline, LV_ALIGN_BOTTOM_MID, 0, -74);
	lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -38);
}

static void render_pattern(lv_obj_t *screen)
{
	const lv_coord_t cell_size = 34;
	const lv_coord_t gap = 2;
	const int columns = 5;
	const int rows = 6;
	const lv_coord_t pattern_width = columns * cell_size + (columns - 1) * gap;
	const lv_coord_t pattern_height = rows * cell_size + (rows - 1) * gap;
	lv_obj_t *title = make_label(screen, "MONO TEST", &lv_font_montserrat_20);
	lv_obj_t *pattern = lv_obj_create(screen);
	lv_obj_t *footer = make_label(screen, "BLACK / WHITE / BORDER", &lv_font_montserrat_14);

	lv_obj_remove_style_all(pattern);
	lv_obj_set_size(pattern, pattern_width, pattern_height);
	lv_obj_align(pattern, LV_ALIGN_CENTER, 0, -2);

	for (int row = 0; row < rows; row++) {
		for (int column = 0; column < columns; column++) {
			lv_obj_t *cell =
				make_box(pattern, cell_size, cell_size, ((row + column) % 2) == 0);

			lv_obj_set_pos(cell, column * (cell_size + gap), row * (cell_size + gap));
		}
	}

	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 32);
	lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -34);
}

static void render_sideways(lv_obj_t *screen)
{
	lv_obj_t *title = make_label(screen, "NORMAL TEXT", &lv_font_montserrat_20);
	lv_obj_t *card = make_rotated_text_image(
		screen, "SIDEWAYS", &lv_font_montserrat_20, CARD_IMAGE_WIDTH, CARD_IMAGE_HEIGHT,
		true, lv_color_white(), lv_color_black(), card_image_buffer,
		sizeof(card_image_buffer), &card_image_dsc);
	lv_obj_t *free_text = make_rotated_text_image(
		screen, "NO BOX", &lv_font_montserrat_20, FREE_IMAGE_WIDTH, FREE_IMAGE_HEIGHT,
		false, lv_color_black(), lv_color_white(), free_image_buffer,
		sizeof(free_image_buffer), &free_image_dsc);
	lv_obj_t *direction = make_label(screen, "CARD: 90 DEG CW", &lv_font_montserrat_14);
	lv_obj_t *footer = make_label(screen, "TEXT: 90 DEG CCW", &lv_font_montserrat_14);

	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);
	lv_obj_set_pos(card, 135 - (CARD_IMAGE_WIDTH / 2), 208 - (CARD_IMAGE_HEIGHT / 2));
	lv_obj_set_pos(free_text, 42 - (FREE_IMAGE_WIDTH / 2),
		       208 - (FREE_IMAGE_HEIGHT / 2));
	lv_obj_align(direction, LV_ALIGN_BOTTOM_MID, 0, -68);
	lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -30);
}

const char *epaper_demo_name(enum epaper_demo_id id)
{
	switch (id) {
	case EPAPER_DEMO_ID_HELLO:
		return "hello";
	case EPAPER_DEMO_ID_STATUS:
		return "status";
	case EPAPER_DEMO_ID_PATTERN:
		return "pattern";
	case EPAPER_DEMO_ID_SIDEWAYS:
		return "sideways";
	default:
		return "unknown";
	}
}

void epaper_demo_render(enum epaper_demo_id id, uint32_t sequence)
{
	lv_obj_t *screen = prepare_screen();

	switch (id) {
	case EPAPER_DEMO_ID_HELLO:
		render_hello(screen);
		break;
	case EPAPER_DEMO_ID_STATUS:
		render_status(screen, sequence);
		break;
	case EPAPER_DEMO_ID_PATTERN:
		render_pattern(screen);
		break;
	case EPAPER_DEMO_ID_SIDEWAYS:
		render_sideways(screen);
		break;
	default:
		render_hello(screen);
		break;
	}

	lv_obj_invalidate(screen);
}
