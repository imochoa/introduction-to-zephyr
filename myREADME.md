# ESP32-S3 DevKitC
- COM for normal cases
- USB for debugging

# On MacOS

 ls /dev/cu.*
/dev/cu.Bluetooth-Incoming-Port
/dev/cu.debug-console
/dev/cu.usbmodem111401
/dev/cu.usbmodem1234561

1. Find the port
    - MacOS `/dev/cu.usbmodem1234561`
    - Test with `uvx esptool --port /dev/cu.usbmodem1234561 chip-id`
1. Put into bootloader mode manually
    1. Hold "BOOT"
    2. Press and release "RST"
    3. Release "BOOT"
1. Run flashing command