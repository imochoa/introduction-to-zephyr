# E-Paper Demo — Agent Context

**Last updated:** 2026-08-18
**Status:** Working on physical hardware; displays LVGL text.

## Purpose

This app drives a WeAct Studio 3.7-inch e-paper module from an ESP32-S3-DevKitC.
The module contains a Good Display GDEY037T03 240x416 monochrome panel with an
UltraChip UC8253 controller. Zephyr does not support UC8253 directly, so the app
contains an out-of-tree display driver and devicetree binding.

The implementation deliberately follows Zephyr's supported
`drivers/display/uc81xx.c` driver and `ultrachip,uc81xx-common.yaml` binding where
the controllers behave alike. Differences are kept explicit below and are based
on the GxEPD2 `GxEPD2_370_GDEY037T03` reference implementation.

## Hardware and wiring

| Signal | ESP32-S3 GPIO | Board label | E-paper pin |
|--------|----------------|-------------|-------------|
| VCC | — | `3V3` | VCC |
| BUSY | 3 | `3` | BUSY |
| RESET | 46 | `46` | RES |
| D/C | 9 | `9` | DC |
| CS | 10 | `10` | CS |
| MOSI | 11 | `11` | SDA/DIN |
| SCLK | 12 | `12` | SCL/CLK |
| GND | — | `G` | GND |

All signals are on J1, with the USB connectors at the bottom. GPIO46 is
`<&gpio1 14 ...>` in devicetree because ESP32-S3 GPIOs 32 and above use gpio1.

**Important:** use the board's `3V3` pin. The `5V` pin on this board is a power
input, not a suitable output for this module.

## Canonical files

```text
10_demo_epaper/
├── CMakeLists.txt
├── prj.conf
├── boards/
│   ├── esp32s3_devkitc.overlay       # canonical UC8253 wiring and USB console
│   ├── esp32s3_devkitc-8179.overlay  # old built-in-driver experiment
│   └── usb_print.overlay             # reusable USB-console fragment
├── drivers/display/uc8253.c
├── dts/bindings/display/ultrachip,uc8253.yaml
└── src/main.c
```

Do not select the overlay in `CMakeLists.txt`. Zephyr automatically discovers
`boards/esp32s3_devkitc.overlay`, and command-line `DTC_OVERLAY_FILE` overrides
remain possible. The canonical overlay must use `compatible =
"ultrachip,uc8253"`; the UC8179 overlay is retained only as an experiment.

## Alignment with Zephyr's UC81xx driver

The custom driver solves shared problems the same way as `uc81xx.c` where
possible:

- implements Zephyr's `display_driver_api` rather than exposing a private app API
- obtains dimensions, BUSY GPIO, MIPI-DBI parent, and SPI settings from DT
- reports `PIXEL_FORMAT_MONO10`, `SCREEN_INFO_MONO_MSB_FIRST`, and
  `SCREEN_INFO_EPD`
- uses `GPIO_ACTIVE_LOW` for the active-low BUSY signal
- uses `SPI_LOCK_ON` and releases MIPI-DBI after each complete command
- expresses the required command/data CS hold as DT `mipi-hold-cs`, allowing
  `MIPI_DBI_SPI_CONFIG_DT()` to construct the SPI operation
- treats blanking transitions idempotently: only a true-to-false transition
  refreshes the panel
- defers refresh while blanked so LVGL can submit a complete frame
- rejects out-of-bounds writes and unsupported pixel formats

### Intentional UC8253/GDEY037T03 differences

| Concern | Supported UC81xx driver | This UC8253 driver |
|---------|-------------------------|--------------------|
| Panel setup | Per-controller quirks and optional DT refresh profiles | Two-byte PSR `[1f 0d]` and full-refresh CDI `0x97` from GxEPD2 |
| BUSY handling | Waits before commands with no timeout | Waits after PON, DRF, and POF as GxEPD2 does; GPIO errors and a 20 s timeout propagate to callers |
| Updates | Uses PTIN/PTL/PTOUT partial windows | Accumulates writes in a 12,480-byte shadow framebuffer and performs a validated full update |
| Old/new RAM | Uses controller auto-copy or quirk handling | Writes the complete frame to DTM1 and DTM2 before a full refresh |
| Initialization | Initializes controller and RAM | Also performs an initial white panel refresh, matching the known-working hardware sequence |

The GDEY037T03 reference driver does support partial-window updates. Partial
refresh is **not implemented here yet**; it is not a known UC8253 hardware
limitation. Add it only after validating PTIN (`0x91`), PTL (`0x90`), PTOUT
(`0x92`), CDI `0xD7`, old-RAM synchronization, and ghosting behavior on the
physical panel.

## Driver behavior

### Commands

| Command | Value | Purpose |
|---------|-------|---------|
| PSR | `0x00` | Two-byte panel setting |
| POF | `0x02` | Power off |
| PON | `0x04` | Power on |
| DTM1 | `0x10` | Previous image RAM |
| DRF | `0x12` | Display refresh |
| DTM2 | `0x13` | Current image RAM |
| CDI | `0x50` | VCOM/data interval (`0x97` for full refresh) |

### Initialization

```text
hardware reset
wait 200 ms
PSR [1e 0d] (soft reset)
wait 1 ms
PSR [1f 0d] (panel configuration)
fill shadow framebuffer white
write white to DTM1 and DTM2
CDI 0x97 -> PON -> wait -> DRF -> wait -> POF -> wait
enter blanked/deferred-update state
```

`uc8253_busy_wait()` must return failures. Never log a BUSY timeout and continue
the command sequence: doing so can send commands while the panel is still busy.

`uc8253_write()` requires byte-aligned monochrome rectangles. It validates pitch,
buffer size, alignment, and bounds before copying rows into the shadow buffer.
The panel width must be divisible by eight.

## LVGL contract

Use 1-bpp tiled flushing:

```config
CONFIG_LVGL=y
CONFIG_LV_Z_BITS_PER_PIXEL=1
CONFIG_LV_COLOR_DEPTH_1=y
CONFIG_LV_Z_VDB_SIZE=100
# CONFIG_LV_Z_FULL_REFRESH is not set
```

Do **not** enable `CONFIG_LV_Z_FULL_REFRESH` with this Zephyr/LVGL version.
Zephyr's `lvgl_flush_cb_mono()` calls `display_blanking_on()` on the first
non-final tile and `display_blanking_off()` after the final tile. A single full
flush is already marked final, so it is not bracketed and the deferred UC8253
frame would not refresh naturally.

Do not manually call `display_blanking_off()` from the application. With tiled
flushing, the LVGL adapter owns blanking and manual calls risk duplicate panel
refreshes. Set an explicit white opaque background and black foreground because
default theme colors are not reliable on a 1-bpp display.

The demo invokes `lv_task_handler()` once because its screen is static. A dynamic
application must call the LVGL handler periodically from its normal UI loop.

## Build and inspect

From the devcontainer workspace root:

```bash
devcontainer exec \
  --workspace-folder . \
  --docker-path podman \
  --docker-compose-path podman-compose \
  -- bash -c 'cd introduction-to-zephyr/workspace/apps/10_demo_epaper && just build'
```

Confirm that the resulting devicetree contains:

```text
compatible = "ultrachip,uc8253"
```

and that `.config` contains:

```text
CONFIG_DT_HAS_ULTRACHIP_UC8253_ENABLED=y
```

Flashing and serial monitoring run on the host. Set `ESP32_PORT` to the actual
`/dev/ttyACM*`, `/dev/ttyUSB*`, or `/dev/cu.*` device.

## Known hardware-debugging history

- Supplying the module from the board's 5 V input pin left it unpowered and
  produced grainy/random pixels; moving VCC to 3.3 V fixed power.
- Incorrect BUSY polarity caused 20-second timeouts; DT must use
  `GPIO_ACTIVE_LOW`.
- This ESP32 MIPI-DBI path must keep CS asserted between a command and its data;
  use the child's `mipi-hold-cs` property.
- Several early failures were wiring-position errors because the J1 silkscreen
  labels GPIO numbers rather than sequential header-pin numbers.

## References

- Zephyr: `/opt/toolchains/zephyr/drivers/display/uc81xx.c`
- Zephyr binding: `/opt/toolchains/zephyr/dts/bindings/display/ultrachip,uc81xx-common.yaml`
- Zephyr LVGL mono adapter: `/opt/toolchains/zephyr/modules/lvgl/lvgl_display_mono.c`
- GxEPD2: `src/gdey/GxEPD2_370_GDEY037T03.cpp`
- WeAct Studio examples: `github.com/WeActStudio/WeActStudio.EpaperModule`
- UC8253 datasheet: `elecrow.com/download/product/DIE01237S/UC8253_Datasheet.pdf`
