#ifndef EPAPER_DEMOS_H_
#define EPAPER_DEMOS_H_

#include <stdint.h>

#include <lvgl.h>

enum epaper_demo_id {
	EPAPER_DEMO_ID_HELLO,
	EPAPER_DEMO_ID_STATUS,
	EPAPER_DEMO_ID_PATTERN,
	EPAPER_DEMO_ID_SIDEWAYS,
	EPAPER_DEMO_ID_COUNT,
};

const char *epaper_demo_name(enum epaper_demo_id id);
void epaper_demo_render(enum epaper_demo_id id, uint32_t sequence);

#endif /* EPAPER_DEMOS_H_ */
