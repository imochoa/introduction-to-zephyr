/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * UC8253 EPD controller driver (e.g. GDEY037T03 240x416 3.7" e-paper).
 *
 * Based on the GxEPD2_370_GDEY037T03 Arduino driver by Jean-Marc Zingg.
 *
 * Key UC8253 traits:
 *   - 2-byte Panel Setting Register (PSR)
 *   - BUSY pin is LOW while busy, HIGH when ready (GPIO_ACTIVE_LOW)
 *   - In BW mode: DTM1=previous, DTM2=current
 *   - CDI (0x50) must be set before each refresh
 *   - busy_wait is ONLY used after PON/DRF/POF — never before commands
 */

// Based on ???
// /opt/toolchains/zephyr/drivers/display/uc81xx.c

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/mipi_dbi.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(uc8253, CONFIG_DISPLAY_LOG_LEVEL);

#define UC8253_CMD_PSR 0x00
#define UC8253_CMD_POF 0x02
#define UC8253_CMD_PON 0x04
#define UC8253_CMD_DTM1 0x10
#define UC8253_CMD_DRF 0x12
#define UC8253_CMD_DTM2 0x13
#define UC8253_CMD_CDI 0x50
#define UC8253_CMD_FLG 0x71

#define UC8253_PIXELS_PER_BYTE 8U
#define UC8253_RESET_DELAY_MS 50U
#define UC8253_BUSY_POLL_MS 10U
#define UC8253_BUSY_TIMEOUT_MS 20000U

struct uc8253_config
{
	const struct device *mipi_dev;
	const struct mipi_dbi_config dbi_config;
	struct gpio_dt_spec busy_gpio;
	uint16_t width;
	uint16_t height;
	const uint8_t *psr;
	uint8_t psr_len;
};

struct uc8253_data
{
	bool blanking_on;
	uint8_t *framebuf;
	size_t framebuf_size;
};

/*
 * GxEPD2 only calls _waitWhileBusy after PON, DRF, and POF — never
 * before sending commands. The controller deasserts BUSY on its own
 * after completing power operations.
 *
 * UC8253 BUSY: LOW = busy, HIGH = ready (GPIO_ACTIVE_LOW in DT).
 * With GPIO_ACTIVE_LOW, gpio_pin_get_dt returns 1 when physical LOW (busy).
 * GxEPD2 adds delay(1) before polling to give the controller time to
 * assert BUSY after a command.
 */
static void uc8253_busy_wait(const struct device *dev)
{
	const struct uc8253_config *config = dev->config;
	uint32_t elapsed = 0;

	/* Give UC8253 time to assert BUSY after command (GxEPD2 does delay(1)) */
	k_sleep(K_MSEC(1));

	while (gpio_pin_get_dt(&config->busy_gpio) > 0)
	{
		k_sleep(K_MSEC(UC8253_BUSY_POLL_MS));
		elapsed += UC8253_BUSY_POLL_MS;
		if (elapsed >= UC8253_BUSY_TIMEOUT_MS)
		{
			LOG_ERR("BUSY timeout after %u ms", elapsed);
			return;
		}
	}

	if (elapsed > 0)
	{
		LOG_INF("BUSY cleared after %u ms", elapsed);
	}
}

static int uc8253_write_cmd(const struct device *dev, uint8_t cmd,
							const uint8_t *data, size_t len)
{
	const struct uc8253_config *config = dev->config;
	int err;

	err = mipi_dbi_command_write(config->mipi_dev, &config->dbi_config,
								 cmd, data, len);
	if (err)
	{
		LOG_ERR("SPI cmd 0x%02X failed: %d (len=%u)", cmd, err,
				(unsigned int)len);
	}

	mipi_dbi_release(config->mipi_dev, &config->dbi_config);
	return err;
}

/*
 * Full refresh — matches GxEPD2 _Update_Full() for GDEY037T03:
 *   CDI(0x97) → PON → waitBusy → DRF → waitBusy → POF → waitBusy
 */
static int uc8253_full_refresh(const struct device *dev)
{
	uint8_t cdi = 0x97;
	int err;

	LOG_INF("Full refresh start");

	err = uc8253_write_cmd(dev, UC8253_CMD_CDI, &cdi, 1);
	if (err)
	{
		return -EIO;
	}

	err = uc8253_write_cmd(dev, UC8253_CMD_PON, NULL, 0);
	if (err)
	{
		return -EIO;
	}
	LOG_INF("PON sent, waiting for BUSY...");
	uc8253_busy_wait(dev);

	err = uc8253_write_cmd(dev, UC8253_CMD_DRF, NULL, 0);
	if (err)
	{
		return -EIO;
	}
	LOG_INF("DRF sent, waiting for BUSY...");
	uc8253_busy_wait(dev);

	err = uc8253_write_cmd(dev, UC8253_CMD_POF, NULL, 0);
	if (err)
	{
		return -EIO;
	}
	LOG_INF("POF sent, waiting for BUSY...");
	uc8253_busy_wait(dev);

	LOG_INF("Full refresh complete");
	return 0;
}

static int uc8253_flush_and_refresh(const struct device *dev)
{
	struct uc8253_data *data = dev->data;

	LOG_INF("Flush %u bytes to DTM1+DTM2",
			(unsigned int)data->framebuf_size);

	if (uc8253_write_cmd(dev, UC8253_CMD_DTM1,
						 data->framebuf, data->framebuf_size))
	{
		return -EIO;
	}
	LOG_INF("DTM1 written");

	if (uc8253_write_cmd(dev, UC8253_CMD_DTM2,
						 data->framebuf, data->framebuf_size))
	{
		return -EIO;
	}
	LOG_INF("DTM2 written");

	return uc8253_full_refresh(dev);
}

static int uc8253_blanking_off(const struct device *dev)
{
	struct uc8253_data *data = dev->data;
	int err = uc8253_flush_and_refresh(dev);

	data->blanking_on = false;
	return err;
}

static int uc8253_blanking_on(const struct device *dev)
{
	struct uc8253_data *data = dev->data;

	data->blanking_on = true;
	return 0;
}

static int uc8253_write(const struct device *dev, const uint16_t x,
						const uint16_t y,
						const struct display_buffer_descriptor *desc,
						const void *buf)
{
	const struct uc8253_config *config = dev->config;
	struct uc8253_data *data = dev->data;
	const uint8_t *src = buf;

	uint16_t src_stride = desc->pitch / UC8253_PIXELS_PER_BYTE;
	uint16_t dst_stride = config->width / UC8253_PIXELS_PER_BYTE;
	uint16_t x_bytes = x / UC8253_PIXELS_PER_BYTE;
	uint16_t w_bytes = desc->width / UC8253_PIXELS_PER_BYTE;

	LOG_DBG("x %u, y %u, w %u, h %u, pitch %u",
			x, y, desc->width, desc->height, desc->pitch);

	__ASSERT(desc->width <= desc->pitch, "Pitch is smaller than width");
	__ASSERT(buf != NULL, "Buffer is not available");
	__ASSERT(!(desc->width % UC8253_PIXELS_PER_BYTE),
			 "Width not multiple of %d", UC8253_PIXELS_PER_BYTE);
	__ASSERT(!(x % UC8253_PIXELS_PER_BYTE),
			 "X not multiple of %d", UC8253_PIXELS_PER_BYTE);

	if ((x + desc->width > config->width) ||
		(y + desc->height > config->height))
	{
		LOG_ERR("Position out of bounds");
		return -EINVAL;
	}

	for (uint16_t row = 0; row < desc->height; row++)
	{
		memcpy(&data->framebuf[(y + row) * dst_stride + x_bytes],
			   &src[row * src_stride],
			   w_bytes);
	}

	if (!data->blanking_on)
	{
		return uc8253_flush_and_refresh(dev);
	}

	return 0;
}

static void uc8253_get_capabilities(const struct device *dev,
									struct display_capabilities *caps)
{
	const struct uc8253_config *config = dev->config;

	memset(caps, 0, sizeof(struct display_capabilities));
	caps->x_resolution = config->width;

	caps->y_resolution = config->height;
	caps->supported_pixel_formats = PIXEL_FORMAT_MONO10;
	caps->current_pixel_format = PIXEL_FORMAT_MONO10;
	caps->screen_info = SCREEN_INFO_MONO_MSB_FIRST | SCREEN_INFO_EPD;
	// caps->display_orientation = DISPLAY_ORIENTATION_ROTATED_90;
}

static int uc8253_set_pixel_format(const struct device *dev,
								   const enum display_pixel_format pf)
{
	if (pf == PIXEL_FORMAT_MONO10)
	{
		return 0;
	}

	LOG_ERR("Unsupported pixel format");
	return -ENOTSUP;
}

/*
 * Init sequence — matches GxEPD2 _InitDisplay() + clearScreen():
 *   1. Hardware reset, wait 200ms (no busy_wait — controller is booting)
 *   2. PSR soft reset [0x1E, 0x0D], wait 1ms
 *   3. PSR config [0x1F, 0x0D]
 *   4. DTM1 + DTM2 (0xFF white), then CDI→PON→DRF→POF with busy_wait
 */
static int uc8253_controller_init(const struct device *dev)
{
	const struct uc8253_config *config = dev->config;
	struct uc8253_data *data = dev->data;
	uint8_t psr_reset[2];

	LOG_INF("Initializing UC8253 (%ux%u)", config->width, config->height);

	mipi_dbi_reset(config->mipi_dev, UC8253_RESET_DELAY_MS);
	k_sleep(K_MSEC(200));

	LOG_INF("Hardware reset done, BUSY pin=%d",
			gpio_pin_get_dt(&config->busy_gpio));

	psr_reset[0] = config->psr[0] & ~0x01u;
	psr_reset[1] = config->psr[1];
	if (uc8253_write_cmd(dev, UC8253_CMD_PSR, psr_reset, 2))
	{
		return -EIO;
	}
	k_sleep(K_MSEC(1));
	LOG_INF("PSR soft reset [0x%02X 0x%02X]", psr_reset[0], psr_reset[1]);

	if (uc8253_write_cmd(dev, UC8253_CMD_PSR,
						 config->psr, config->psr_len))
	{
		return -EIO;
	}
	LOG_INF("PSR config [0x%02X 0x%02X]", config->psr[0], config->psr[1]);

	memset(data->framebuf, 0xFF, data->framebuf_size);

	LOG_INF("Clearing display to white...");
	if (uc8253_flush_and_refresh(dev))
	{
		LOG_ERR("Initial clear failed");
		return -EIO;
	}

	LOG_INF("Init complete");
	data->blanking_on = true;
	return 0;
}

static int uc8253_init(const struct device *dev)
{
	const struct uc8253_config *config = dev->config;

	LOG_INF("UC8253 driver init");

	if (!device_is_ready(config->mipi_dev))
	{
		LOG_ERR("MIPI DBI device not ready");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&config->busy_gpio))
	{
		LOG_ERR("BUSY GPIO device not ready");
		return -ENODEV;
	}

	if (gpio_pin_configure_dt(&config->busy_gpio, GPIO_INPUT))
	{
		LOG_ERR("Failed to configure BUSY GPIO");
		return -EIO;
	}

	LOG_INF("MIPI DBI and GPIO ready, BUSY pin=%d",
			gpio_pin_get_dt(&config->busy_gpio));

	return uc8253_controller_init(dev);
}

static const struct display_driver_api uc8253_driver_api = {
	.blanking_on = uc8253_blanking_on,
	.blanking_off = uc8253_blanking_off,
	.write = uc8253_write,
	.get_capabilities = uc8253_get_capabilities,
	.set_pixel_format = uc8253_set_pixel_format,
};

#define UC8253_BUF_SIZE(n) \
	((DT_PROP(n, width) * DT_PROP(n, height)) / UC8253_PIXELS_PER_BYTE)

#define UC8253_DEFINE(n)                                    \
	static uint8_t uc8253_psr_##n[] = DT_PROP(n, psr);      \
	static uint8_t uc8253_framebuf_##n[UC8253_BUF_SIZE(n)]; \
                                                            \
	static const struct uc8253_config uc8253_cfg_##n = {    \
		.mipi_dev = DEVICE_DT_GET(DT_PARENT(n)),            \
		.dbi_config = {                                     \
			.mode = MIPI_DBI_MODE_SPI_4WIRE,                \
			.config = MIPI_DBI_SPI_CONFIG_DT(               \
				n,                                          \
				SPI_OP_MODE_MASTER | SPI_WORD_SET(8) |      \
					SPI_HOLD_ON_CS | SPI_LOCK_ON,           \
				0),                                         \
		},                                                  \
		.busy_gpio = GPIO_DT_SPEC_GET(n, busy_gpios),       \
		.width = DT_PROP(n, width),                         \
		.height = DT_PROP(n, height),                       \
		.psr = uc8253_psr_##n,                              \
		.psr_len = sizeof(uc8253_psr_##n),                  \
	};                                                      \
                                                            \
	static struct uc8253_data uc8253_data_##n = {           \
		.framebuf = uc8253_framebuf_##n,                    \
		.framebuf_size = UC8253_BUF_SIZE(n),                \
	};                                                      \
                                                            \
	DEVICE_DT_DEFINE(n, uc8253_init, NULL,                  \
					 &uc8253_data_##n,                      \
					 &uc8253_cfg_##n,                       \
					 POST_KERNEL,                           \
					 CONFIG_DISPLAY_INIT_PRIORITY,          \
					 &uc8253_driver_api);

DT_FOREACH_STATUS_OKAY(ultrachip_uc8253, UC8253_DEFINE)
