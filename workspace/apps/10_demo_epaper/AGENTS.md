# E-Paper Demo — AI Agent Context

**Last Updated:** 2026-03-16
**Status:** BUSY polarity fixed (was inverted — caused all timeouts). Ready to flash.

## Root Cause of "Grainy" Display (Sessions 1-3)

The display VCC was connected to the ESP32-S3-DevKitC 5V pin, which is a
**power INPUT** (for powering the devkit via USB), not an output. The e-paper
module was never powered. All "grainy" troubleshooting across 3 sessions was
chasing a phantom — the controller was dead, and the e-ink particles were in
their factory residual state.

## Root Cause of BUSY Timeouts (Session 5)

After fixing power (3.3V), all three busy_wait calls (PON, DRF, POF) timed out
at 20 seconds. The BUSY pin polarity was **inverted** in the driver:

- **UC8253 actual behavior**: BUSY LOW = busy, HIGH = ready
  (confirmed by both Adafruit `while (!digitalRead)` and GxEPD2 `busy_level=LOW`)
- **Our driver had**: `GPIO_ACTIVE_HIGH` in DT overlay, so `gpio_pin_get_dt`
  returned 1 when physical HIGH (idle/ready), and the `while > 0` loop waited
  forever on the idle state.

**Fix**: Changed DT overlay to `GPIO_ACTIVE_LOW`. Now `gpio_pin_get_dt` returns
1 when physical LOW (busy) and the loop correctly waits during busy state.
Also added 1ms pre-poll delay matching GxEPD2's `delay(1)` in `_waitWhileBusy`.

## What This Project Is

A Zephyr RTOS app driving a **WeAct Studio 3.7" E-Paper Module** (GDEY037T03 panel, **UC8253** controller) from an ESP32-S3-DevKitC. Uses an out-of-tree UC8253 display driver since Zephyr has no built-in support for this controller.

Started as a copy of `10_demo_display/` (an ST7735R TFT demo). All TFT-specific code has been replaced.

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

These pins match the WeAct Arduino example defaults:

| Signal | ESP32-S3 GPIO | Zephyr DT reference | E-Paper pin |
|--------|---------------|---------------------|-------------|
| MOSI   | GPIO11        | `SPIM2_MOSI_GPIO11` | SDA/DIN     |
| SCLK   | GPIO12        | `SPIM2_SCLK_GPIO12` | SCL/CLK     |
| CS     | GPIO10        | `SPIM2_CSEL_GPIO10` | CS          |
| DC     | GPIO9         | `&gpio0 9`          | DC          |
| RST    | GPIO46        | `&gpio1 14`         | RES         |
| BUSY   | GPIO3         | `&gpio0 3`          | BUSY        |

Note: ESP32-S3 GPIOs >= 32 are on `&gpio1` with offset -32 (GPIO46 = `&gpio1 14`).

## File Structure

```
10_demo_epaper/
  CMakeLists.txt                         # Build config — adds DTS_ROOT, compiles driver
  prj.conf                               # Kconfig — display, SPI, MIPI_DBI, logging
  boards/
    esp32s3_devkitc.overlay              # DT overlay — SPI pinctrl, MIPI-DBI, UC8253 node
  dts/bindings/display/
    ultrachip,uc8253.yaml                # DT binding — defines UC8253 properties
  drivers/display/
    uc8253.c                             # OUT-OF-TREE UC8253 driver (~330 lines)
  src/
    main.c                               # Test app — white/black/checkerboard pattern
  build/                                  # Last build output (218 steps, succeeded)
```

## UC8253 Controller — Technical Details

The UC8253 is in the UC81xx family but has differences that prevent using Zephyr's built-in `uc81xx` driver:

| Aspect | UC81xx (Zephyr built-in) | UC8253 (this driver) |
|--------|--------------------------|----------------------|
| PSR register | 1 byte | **2 bytes** (`0x1F, 0x0D`) |
| Partial window (PTL) | Supported, used by driver | **Not supported** |
| RAM addressing | Via PTL commands | Full framebuffer write only |
| BUSY polarity | Active low | Active low (same) — **LOW = busy, HIGH = ready. DT uses GPIO_ACTIVE_LOW.** |
| Core commands | PON/POF/DRF/DTM1/DTM2 | Same command codes |
| GET_STATUS quirk | Not needed | **Adafruit sends 0x71 during busy_wait (reads response via MISO). GxEPD2 uses pure GPIO polling. Our driver uses GPIO-only (no MISO).** |

### Command Map

| Command | Code | Description |
|---------|------|-------------|
| PSR     | 0x00 | Panel Setting Register (2 bytes for UC8253) |
| POF     | 0x02 | Power Off |
| PON     | 0x04 | Power On |
| DSLP    | 0x07 | Deep Sleep (key: 0xA5) |
| DTM1    | 0x10 | Write RAM buffer 1 (old/black) |
| DRF     | 0x12 | Display Refresh — drives e-ink particles |
| DTM2    | 0x13 | Write RAM buffer 2 (new/red on color panels, used as primary on B/W) |
| FLG     | 0x71 | Get Status — **Adafruit sends this during busy_wait and reads response. GxEPD2 does NOT use it. Our driver uses GPIO polling only (no MISO).** |

### Display Properties

- 240x416 pixels, 1-bit monochrome (PIXEL_FORMAT_MONO10)
- MSB first bit ordering (SCREEN_INFO_MONO_MSB_FIRST)
- Framebuffer size: 12,480 bytes (240 * 416 / 8)
- PSR default: `[0x1F, 0x0D]` (from GxEPD2_370_GDEY037T03 reference driver)

## Current Driver Architecture

The driver (`drivers/display/uc8253.c`) implements Zephyr's `display_driver_api`:

- **`uc8253_init`** — Validates MIPI-DBI and BUSY GPIO readiness, calls `controller_init`
- **`uc8253_controller_init`** — Hardware reset, PON, PSR(2 bytes), clear RAM (DTM1+DTM2 = 0xFF), physical refresh
- **`uc8253_busy_wait`** — Polls BUSY GPIO (GPIO_ACTIVE_LOW: returns 1 when busy). 1ms pre-delay before polling, matching GxEPD2's _waitWhileBusy.
- **`uc8253_write`** — Copies partial rect into shadow framebuffer, flushes entire buffer via DTM2 if blanking is off
- **`uc8253_blanking_off`** — Sends full framebuffer + triggers PON→DRF→wait→POF refresh
- **`uc8253_blanking_on`** — Sets flag to defer refresh (buffer updates without display refresh)
- **`uc8253_get_capabilities`** — Reports MONO10, width/height, EPD screen info

The driver maintains a static shadow framebuffer in MCU RAM. Partial `display_write()` calls update the shadow; the entire buffer is sent to the controller on refresh.

### Driver Build Integration

The driver is built as an out-of-tree source, NOT a Zephyr module:

```cmake
# CMakeLists.txt
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})          # For DT bindings
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
target_sources(app PRIVATE src/main.c drivers/display/uc8253.c)  # Driver compiled directly
```

There is no Kconfig or `ZEPHYR_EXTRA_MODULES` — the driver is just another source file.
The DT binding at `dts/bindings/display/ultrachip,uc8253.yaml` is found via `DTS_ROOT`.

## The Problem — Grainy Display

After flashing, the display shows grainy/noisy pixels instead of the expected test pattern (white → black → checkerboard). This happened across multiple code revisions:

1. **First attempt** (with LVGL): grainy
2. **After adding GET_STATUS to busy_wait**: grainy
3. **After adding initial refresh in controller_init**: grainy
4. **After replacing LVGL with raw display API**: still grainy

"Grainy" means the e-ink particles are in a random/residual state — as if the controller never received any commands, or the commands aren't reaching the display.

## What Has Been Tried

### Fixes Applied (all in current code)

1. **GET_STATUS in busy_wait** — Added `mipi_dbi_command_write(FLG)` inside the BUSY polling loop, matching the Adafruit reference. Without this, BUSY may never deassert on UC8253.

2. **Initial physical refresh in controller_init** — After writing 0xFF to both DTM1 and DTM2, the driver now calls `uc8253_update_display()` (PON → DRF → wait → POF). Previously it only wrote RAM without triggering a refresh, so e-ink particles were never driven.

3. **Removed LVGL** — LVGL flushes in horizontal stripes, each triggering a full e-paper refresh cycle. Replaced with direct `display_write()` calls using a single full-frame buffer.

4. **Increased heap** — `CONFIG_HEAP_MEM_POOL_SIZE=16384` for the test pattern's `k_malloc`.

### What Has NOT Been Tried

These are the prime suspects for why the display is still grainy:

1. **SPI communication verification** — No confirmation that SPI data actually reaches the display controller. Could check with:
   - Serial output (`printk`) to verify the code path executes
   - Logic analyzer on SPI lines
   - Add return value checking on every `mipi_dbi_command_write` call and log errors

2. **Complete init sequence comparison** — The Arduino reference (GxEPD2 / Adafruit) may set additional registers during init that our driver skips. Candidates:
   - `TRES` (0x61) — Resolution Setting (width/height explicitly told to controller)
   - `CDI` (0x50) — VCOM and Data Interval Setting
   - `TCON` (0x60) — TCON Setting
   - `PLL` (0x30) — PLL Control
   - `PWR` (0x01) — Power Setting
   - The Adafruit UC8253 library's `powerUp()` and `update()` methods may contain extra steps

3. **PSR values** — The default `[0xCF, 0x8D]` comes from the Adafruit reference, but the specific GDEY037T03 panel revision might need different values. The WeAct Arduino example may use different PSR bytes.

4. **SPI mode/frequency** — Currently 4MHz, SPI mode 0. The UC8253 datasheet should be consulted for max frequency and required SPI mode (CPOL/CPHA).

5. **CS pin handling** — The MIPI-DBI layer manages CS via `SPIM2_CSEL_GPIO10`. If the Zephyr SPI driver doesn't toggle CS correctly for the multi-byte command+data pattern UC8253 expects, communication would fail silently.

6. **Reset timing** — Current reset delay is 10ms. Some e-paper panels need longer (50-100ms) after reset before accepting commands.

7. **Power sequencing** — The UC8253 may need explicit PWR (0x01) register configuration before PON. The Adafruit library may handle this internally.

## Key External References

| Resource | URL | Notes |
|----------|-----|-------|
| WeAct Arduino example | `github.com/WeActStudio/WeActStudio.EpaperModule/blob/master/Example/EpaperModuleTest_Arduino_ESP32S3/` | Uses GxEPD2 library |
| Adafruit UC8253 driver | `github.com/adafruit/Adafruit_EPD/blob/master/src/drivers/Adafruit_UC8253.h` | Reference for command sequences |
| Adafruit EPD base class | `github.com/adafruit/Adafruit_EPD/blob/master/src/Adafruit_EPD.cpp` | `powerUp()`, `update()`, `powerDown()` flow |
| UC8253 datasheet | `elecrow.com/download/product/DIE01237S/UC8253_Datasheet.pdf` | Official register reference |
| Zephyr UC81xx driver | `/opt/toolchains/zephyr/drivers/display/uc81xx/` (inside container) | Structural reference for similar controllers |
| Zephyr SSD16xx driver | `/opt/toolchains/zephyr/drivers/display/ssd16xx.c` (inside container) | Another e-paper driver reference |

## Recommended Next Steps for Debugging

**Priority order — start with the cheapest/most likely:**

### Step 1: Verify SPI is working at all
Add `printk` / `LOG_ERR` return value checks after every `mipi_dbi_command_write` in the init sequence. If SPI writes are failing, the rest doesn't matter. Build and check serial output.

### Step 2: Compare init sequence with Arduino reference
Fetch the actual Adafruit UC8253 source (the `.h` file linked above) and compare the `begin()` / `powerUp()` / `update()` flow with our `controller_init`. Look for missing register writes, different ordering, or different PSR values. The WeAct Arduino example (linked above) may also use panel-specific init tables.

### Step 3: Add missing registers
Based on Step 2, add any missing register writes (TRES, CDI, PWR, PLL, TCON). These typically go between PSR and the first DTM write.

### Step 4: Try slower SPI and longer delays
Drop SPI frequency to 1MHz (`mipi-max-frequency = <1000000>` in overlay). Increase reset delay to 50ms. Increase post-PON delay to 200ms. If this helps, the issue is timing.

### Step 5: Logic analyzer
If software debugging doesn't help, put a logic analyzer on MOSI/SCLK/CS/DC to verify the actual SPI traffic matches what the Arduino produces.

## Build Command

```bash
# From repo root (host), uses devcontainer:
just intro ex-10-epaper-build

# Or raw:
devcontainer exec --workspace-folder . --docker-path podman --docker-compose-path podman-compose -- \
  bash -c 'cd introduction-to-zephyr/workspace/apps/10_demo_epaper && \
  "${VIRTUAL_ENV}/bin/python" -m west build -p always -b esp32s3_devkitc/esp32s3/procpu -- \
  -DDTC_OVERLAY_FILE=./boards/esp32s3_devkitc.overlay'
```

## Flash Command

```bash
# From host:
just flash /dev/cu.usbmodem101

# Or raw:
python -m esptool --port "/dev/cu.usbmodem101" --chip auto --baud 921600 \
  --before default_reset --after hard_reset write_flash -u \
  --flash_mode keep --flash_freq 40m --flash_size detect 0x0 \
  introduction-to-zephyr/workspace/apps/10_demo_epaper/build/zephyr/zephyr.bin
```

## Session History

| Session | What happened | Outcome |
|---------|---------------|---------|
| `ses_30d427989ffedxikA21MV9UODH` | Created UC8253 driver from scratch, DT binding, overlay, LVGL-based main.c. Build succeeded. User flashed — screen grainy. Agent started fixing (GET_STATUS in busy_wait done, then froze during controller_init fix). | Driver created, but display grainy on first flash |
| `ses_30d099b9affeIQw65IGlGZAbxk` | Picked up remaining tasks. Added init refresh, replaced LVGL with raw display API, rebuilt. User flashed — still grainy. Agent started researching deeper (Adafruit/GxEPD2 init sequences), fired 2 background research tasks, then froze. Background results were never collected. | All known fixes applied, display still grainy |
| `ses_30d00b9bdffewSyXetYLuwva5v` | Created AGENTS.md. Rewrote driver based on GxEPD2: fixed PSR to [0x1F,0x0D], added CDI(0x97), soft reset, DTM1+DTM2 writes. Build succeeded. Compaction crashed session before flash. | Build ready, never flashed |
| `ses_30cc4c849ffe2WVxkkVKmjFPgH` | Discovered VCC was on 5V input pin (unpowered display). Fixed compaction config. Removed spurious GET_STATUS from busy_wait (GxEPD2 uses pure GPIO polling). Rebuilt. | Awaiting flash test with correct power |
| Session 5 (current) | Flashed with 3.3V — BUSY timeout on all PON/DRF/POF. Found BUSY polarity inverted: GPIO_ACTIVE_HIGH→LOW. Added 1ms pre-poll delay. Confirmed init sequence matches GxEPD2 exactly. Build succeeded. | Ready to flash |

## Reference Comparison Summary

Init sequence matches GxEPD2_370_GDEY037T03 exactly:
- PSR soft reset [0x1E, 0x0D] + wait 1ms + PSR config [0x1F, 0x0D] ✅
- CDI(0x97) → PON → DRF → wait → POF matches GxEPD2 _Update_Full() ✅
- DTM1 + DTM2 full buffer writes ✅

Key difference fixed: Adafruit sends GET_STATUS (0x71) during busy_wait, our driver uses GPIO-only (no MISO).
BUSY polarity fixed: GPIO_ACTIVE_LOW (LOW = busy, HIGH = ready), matching both Adafruit and GxEPD2.

## If Display Still Doesn't Work After BUSY Fix

Priority debugging steps:
1. **Check serial output** — BUSY should now show "BUSY cleared after N ms" (not timeout)
2. **If BUSY still times out** — wiring issue on GPIO3, or BUSY pin not connected
3. **If BUSY clears but display is blank** — SPI data issue. Try reducing `mipi-max-frequency` to `<500000>` in overlay
4. **If display shows wrong pattern** — bit ordering or buffer format issue
5. **Logic analyzer** on MOSI/SCLK/CS/DC to verify actual SPI traffic
