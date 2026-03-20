# E-Paper Demo — AI Agent Context

**Last Updated:** 2026-03-16
**Status:** WORKING — displays LVGL text on physical hardware.

## What This Project Is

A Zephyr RTOS app driving a **WeAct Studio 3.7" E-Paper Module** (GDEY037T03 panel, **UC8253** controller) from an ESP32-S3-DevKitC. Uses an out-of-tree UC8253 display driver with LVGL for text/graphics rendering.

## Hardware

| Component | Detail |
|-----------|--------|
| MCU board | ESP32-S3-DevKitC |
| Display | WeAct Studio 3.7" E-Paper Module |
| Panel | GDEY037T03 (240x416 pixels, B/W monochrome) |
| Controller IC | UltraChip UC8253 |
| Interface | 4-wire SPI (write-only, no MISO) |
| Refresh time | ~2 seconds for full refresh |

### Wiring (ESP32-S3 to E-Paper Module)

All signals are on the **J1 header** (left side, USB ports at bottom). The board silkscreen prints GPIO numbers, not sequential pin numbers.

| Signal | GPIO | J1 board label | E-Paper pin |
|--------|------|----------------|-------------|
| VCC    | —    | `3V3` (top)    | VCC         |
| BUSY   | 3    | `3`            | BUSY        |
| RST    | 46   | `46`           | RES         |
| DC     | 9    | `9`            | DC          |
| CS     | 10   | `10`           | CS          |
| MOSI   | 11   | `11`           | SDA/DIN     |
| SCLK   | 12   | `12`           | SCL/CLK     |
| GND    | —    | `G` (bottom)   | GND         |

**CRITICAL**: The ESP32-S3-DevKitC `5V` pin is a **power input**, not output. Always use `3V3` for the display VCC. Using 5V leaves the display completely unpowered.

The six signal pins (labels 3, 46, 9, 10, 11, 12) are a contiguous block on J1. Note: ESP32-S3 GPIOs ≥ 32 are on `&gpio1` with offset -32 in Zephyr DT (GPIO46 = `&gpio1 14`).

## File Structure

```
10_demo_epaper/
  CMakeLists.txt                         # Build config — adds DTS_ROOT, compiles driver as app source
  prj.conf                               # Kconfig — display, SPI, MIPI_DBI, LVGL (1bpp), logging
  boards/
    esp32s3_devkitc.overlay              # DT overlay — SPI pinctrl, MIPI-DBI, UC8253 node
  dts/bindings/display/
    ultrachip,uc8253.yaml                # DT binding — defines UC8253 properties
  drivers/display/
    uc8253.c                             # OUT-OF-TREE UC8253 driver (~380 lines)
  src/
    main.c                               # App — LVGL label "Hello, World!" centered on screen
  build/                                 # Last build output (419 steps, succeeded)
```

## UC8253 Controller — Technical Details

The UC8253 cannot use Zephyr's built-in `uc81xx` driver:

| Aspect | UC81xx (Zephyr built-in) | UC8253 (this driver) |
|--------|--------------------------|----------------------|
| PSR register | 1 byte | **2 bytes** (`0x1F, 0x0D`) |
| Partial window (PTL) | Supported | Not supported — full framebuffer write only |
| BUSY polarity | Active low | Active low — **LOW = busy, HIGH = ready. DT uses `GPIO_ACTIVE_LOW`.** |
| Core commands | PON/POF/DRF/DTM1/DTM2 | Same command codes |

### Command Map

| Command | Code | Description |
|---------|------|-------------|
| PSR     | 0x00 | Panel Setting Register (2 bytes for UC8253) |
| POF     | 0x02 | Power Off |
| PON     | 0x04 | Power On |
| DSLP    | 0x07 | Deep Sleep (key: 0xA5) |
| DTM1    | 0x10 | Write old data buffer |
| DRF     | 0x12 | Display Refresh — drives e-ink particles |
| DTM2    | 0x13 | Write new data buffer (primary in B/W mode) |
| CDI     | 0x50 | VCOM and data interval — 0x97 for full refresh |

### Display Properties

- 240×416 pixels, 1-bit monochrome (`PIXEL_FORMAT_MONO10`)
- MSB first bit ordering (`SCREEN_INFO_MONO_MSB_FIRST`)
- Framebuffer size: 12,480 bytes (240 × 416 / 8)
- PSR: `[0x1F, 0x0D]` (from GxEPD2_370_GDEY037T03 reference driver)

## Driver Architecture

The driver (`drivers/display/uc8253.c`) implements Zephyr's `display_driver_api`:

- **`uc8253_init`** — Validates MIPI-DBI and BUSY GPIO readiness, calls `controller_init`
- **`uc8253_controller_init`** — Hardware reset (50ms), PSR soft reset + config, clears RAM to 0xFF (white), does initial full refresh, sets `blanking_on = true`
- **`uc8253_busy_wait`** — Polls BUSY GPIO with 1ms pre-delay (matching GxEPD2's `delay(1)`). `GPIO_ACTIVE_LOW` means `gpio_pin_get_dt` returns 1 when physically LOW (busy).
- **`uc8253_write`** — Copies rect into shadow framebuffer. If `blanking_on == false`, immediately flushes and refreshes.
- **`uc8253_blanking_on`** — Sets flag to defer refresh. LVGL calls this before starting a frame.
- **`uc8253_blanking_off`** — If `blanking_on` was true, flushes full framebuffer via DTM1+DTM2 and runs CDI→PON→DRF→POF refresh cycle. LVGL calls this after the last flush of a frame.
- **`uc8253_get_capabilities`** — Reports `PIXEL_FORMAT_MONO10`, dimensions, `SCREEN_INFO_MONO_MSB_FIRST | SCREEN_INFO_EPD`

The driver maintains a static shadow framebuffer in MCU RAM (12,480 bytes). All partial writes accumulate there; the entire buffer is sent on each refresh.

### Init Sequence (matches GxEPD2_370_GDEY037T03 exactly)

```
Hardware reset → wait 200ms
PSR soft reset [0x1E, 0x0D] → wait 1ms
PSR config [0x1F, 0x0D]
DTM1(0xFF) + DTM2(0xFF)  ← clear to white
CDI(0x97) → PON → busy_wait → DRF → busy_wait → POF → busy_wait
```

### Driver Build Integration

The driver is compiled directly as an app source file, not a Zephyr module:

```cmake
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
target_sources(app PRIVATE src/main.c drivers/display/uc8253.c)
```

The DT binding at `dts/bindings/display/ultrachip,uc8253.yaml` is found via `DTS_ROOT`.

## LVGL Integration

The app uses LVGL (via Zephyr's module) for text rendering. Key config in `prj.conf`:

```
CONFIG_LVGL=y
CONFIG_LV_Z_BITS_PER_PIXEL=1      # 1bpp monochrome
CONFIG_LV_Z_MEM_POOL_SIZE=8192
CONFIG_LV_CONF_MINIMAL=y
CONFIG_LV_USE_LABEL=y
CONFIG_LV_FONT_MONTSERRAT_14=y
```

**Do NOT add `CONFIG_LV_Z_FULL_REFRESH=y`.** With full refresh, LVGL sends a single flush call that is simultaneously the first and last (`is_last = true`). Zephyr's `lvgl_flush_cb_mono` EPD blanking logic requires at least two flush calls: it only calls `display_blanking_on()` when `!is_last`, so the flag never gets set, and `display_blanking_off()` (which triggers the e-paper refresh) is never called. Without full refresh, LVGL sends ~10 partial flush calls, correctly bracketing them with `blanking_on`/`blanking_off`.

`main.c` usage pattern:
```c
lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);
lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
lv_obj_t *label = lv_label_create(lv_scr_act());
lv_obj_set_style_text_color(label, lv_color_black(), 0);
lv_label_set_text(label, "Hello, World!");
lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
lv_task_handler();  // triggers flush → EPD refresh
```

Always set explicit white background and black text. LVGL's default theme colors on a 1bpp display are unpredictable otherwise.

## Key Bugs Fixed (history)

| Bug | Symptom | Fix |
|-----|---------|-----|
| VCC on 5V pin (input-only) | Grainy/random pixels — display never powered | Move VCC wire to `3V3` pin |
| BUSY polarity inverted | All `busy_wait` calls timed out at 20s | Change overlay to `GPIO_ACTIVE_LOW` |
| Missing `SPI_HOLD_ON_CS` | Commands sent with no data (CS deasserted between cmd and data bytes) | Add `SPI_HOLD_ON_CS \| SPI_LOCK_ON` to SPI config in driver |
| `CONFIG_LV_Z_FULL_REFRESH=y` | Screen stayed white — `display_blanking_off()` never called | Remove that config option |

## Key External References

| Resource | URL |
|----------|-----|
| WeAct Arduino example | `github.com/WeActStudio/WeActStudio.EpaperModule` |
| GxEPD2 GDEY037T03 driver | Reference for init sequence and PSR values |
| UC8253 datasheet | `elecrow.com/download/product/DIE01237S/UC8253_Datasheet.pdf` |
| Zephyr LVGL mono flush source | `/opt/toolchains/zephyr/modules/lvgl/lvgl_display_mono.c` (inside container) |

## Build & Flash

```bash
# Build (from repo root on host):
just intro ex-10-epaper-build

# Flash (from repo root on host):
just flash /dev/cu.usbmodem101
```

## Session History

| Session | What happened | Outcome |
|---------|---------------|---------|
| `ses_30d427989ffedxikA21MV9UODH` | Created UC8253 driver, DT binding, overlay, LVGL main.c. Build succeeded. User flashed — screen grainy. | Driver created, display grainy |
| `ses_30d099b9affeIQw65IGlGZAbxk` | Added init refresh, replaced LVGL with raw display API, rebuilt. User flashed — still grainy. | Still grainy |
| `ses_30d00b9bdffewSyXetYLuwva5v` | Rewrote driver based on GxEPD2: PSR [0x1F,0x0D], CDI(0x97), soft reset, DTM1+DTM2. Build succeeded. Session crashed before flash. | Build ready, never flashed |
| `ses_30cc4c849ffe2WVxkkVKmjFPgH` | Discovered VCC was on 5V input pin. Fixed. Removed spurious GET_STATUS from busy_wait. Rebuilt. | Fixed power, awaiting flash |
| Session 5 | Flashed with 3.3V — BUSY timeout on all PON/DRF/POF. Found BUSY polarity inverted (GPIO_ACTIVE_HIGH→LOW). Added 1ms pre-poll delay. Build succeeded. | BUSY fixed, ready to flash |
| Session 6 | Confirmed wiring using J1 physical pin positions. User had wires in wrong positions. After correction, display worked. | **Display working** |
| Session 7 | Added LVGL text rendering. Fixed `CONFIG_LV_Z_FULL_REFRESH` bug (EPD blanking never fired with full refresh). "Hello, World!" displays correctly. | **LVGL text working** |
