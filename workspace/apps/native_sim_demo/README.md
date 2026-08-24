# native_sim demo

This application runs Zephyr as a Linux process inside the devcontainer. It uses a producer thread and a message queue, so no ESP32-S3 hardware or devicetree overlay is required.

From the repository root:

```sh
just intro::native-sim-run
```

To build without running:

```sh
just intro::native-sim-build
```

The executable is written to `build/native_sim/zephyr/zephyr.exe`. Press Ctrl+C to stop it.

`native_sim` recompiles the application for the Linux host; it does not emulate the ESP32-S3 processor or its peripherals.
