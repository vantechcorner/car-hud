# ESP32 Classic Bridge (BT3.0 -> UART)

This firmware is for an ESP32 classic board (ESP32-WROOM/WROVER class) that connects to a Bluetooth Classic ELM327 adapter (e.g. Vgate iCar Pro BT3.0) and forwards parsed OBD lines to the ESP32-S3 HUD over UART.

## Why this bridge exists

`ESP32-S3` supports BLE only, while `Vgate iCar Pro BT3.0` uses Bluetooth Classic.

## Wiring

- Bridge `TX` -> HUD `RX`
- Bridge `RX` -> HUD `TX` (optional, reserved for future use)
- Bridge `GND` -> HUD `GND`

Default pins from `platformio.ini`:

- Bridge UART TX: `GPIO17`
- Bridge UART RX: `GPIO16`

HUD side defaults in `src/main.cpp` when bridge mode is enabled:

- HUD RX: `GPIO16`
- HUD TX: `GPIO15`

You can override these by build flags.

## Build and upload

From this `bridge` folder:

```bash
pio run -e esp32_bridge_bt3
pio run -e esp32_bridge_bt3 -t upload
pio device monitor -b 115200
```

## Adapter config

In `bridge/platformio.ini` you can set both. The firmware **connects by name first**, then tries **MAC** only if the name fails (saves time when name works).

- `-D OBD_BT_NAME=\"V-LINK\"` (tried first)
- `-D OBD_BT_MAC=\"AA:BB:CC:DD:EE:FF\"` (optional fallback)

## Protocol to HUD

The bridge forwards OBD responses as text lines such as:

`41 0C 1A F8`

`41 0D 3C`

The HUD parser reads these lines directly.

## Serial: `ASSERT_WARN(1 8), in lc_task.c`

If you see this line on the ESP32 classic serial monitor while Bluetooth SPP is active, it usually comes from **Espressif Bluedroid** (link-controller task), not from application code. It is often **benign** when traffic is frequent (e.g. polling RPM every 100 ms). If the HUD still receives `41 ...` lines and the car data looks correct, you can ignore it. If you see disconnects or instability, try slightly increasing poll intervals in `bridge/src/main.cpp` (`FAST_INTERVAL`, etc.).
