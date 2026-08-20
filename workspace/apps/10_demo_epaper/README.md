# UC8253 e-paper LVGL demo

This app drives a 240x416 GDEY037T03 e-paper panel from an ESP32-S3-DevKitC. It
uses LVGL with the app's UC8253 display driver.

The default build cycles through four screens every 10 seconds:

- a minimal hello card
- a status screen
- a monochrome test pattern
- a black text card rotated 90 degrees

The screens are ordinary functions in `src/demos.c`. Add another function there
and include it in `epaper_demo_render()` to add a demo.

## Select what runs

The defaults in `prj.conf` are:

```config
CONFIG_EPAPER_DEMO_CYCLE=y
CONFIG_EPAPER_DEMO_INTERVAL_SECONDS=10
```

To compile one static screen, replace those lines with one of
`CONFIG_EPAPER_DEMO_HELLO`, `CONFIG_EPAPER_DEMO_STATUS`,
`CONFIG_EPAPER_DEMO_PATTERN`, or `CONFIG_EPAPER_DEMO_SIDEWAYS`. Set only one.
For example, a build containing only the sideways screen uses:

```config
CONFIG_EPAPER_DEMO_SIDEWAYS=y
```

The same options are available through:

```bash
just intro ex-10-epaper-menuconfig
```

## Build

From the outer `zephyr-esp32-devcontainer` repository:

```bash
just intro ex-10-epaper-build
```

The flash image is written to `build/zephyr/zephyr.bin`.

## LVGL flushing

Keep `CONFIG_LV_Z_VDB_SIZE` below 100 and leave `CONFIG_LV_Z_FULL_REFRESH`
disabled. This app uses a 10% buffer, which makes LVGL send the frame in tiles.
Zephyr brackets those tiles with `display_blanking_on()` and
`display_blanking_off()`; the final call starts the physical e-paper refresh.
A one-buffer full-screen flush skips that transition with this Zephyr version,
leaving the panel white.
