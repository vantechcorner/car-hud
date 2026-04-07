# Car HUD

DIY heads-up display: live vehicle data on a **round 240×240** ESP32-S3 display. Data comes from an **ELM327** OBD-II adapter. If the adapter is **Bluetooth Classic** (e.g. BT 3.0), an **ESP32 “bridge”** (NodeMCU-class board) reads the dongle and forwards frames to the HUD over **UART** or **WiFi UDP**.

UI is built with **LVGL** (see also [LVGL Pro](https://pro.lvgl.io/) for the XML/tooling workflow used in `lib/hud_ui/`).

For implementation details, wiring, and PlatformIO env names, see **[HANDOFF.md](HANDOFF.md)**.

## Features

- **OBD-II** mode `01` telemetry: RPM, vehicle speed, coolant temperature, control-module voltage  
- **ESP32-S3** round LCD (default build: Waveshare **ESP32-S3-Touch-LCD-1.28**)  
- **Path A — BLE:** NimBLE to ELM327 **BLE** (when the dongle supports it)  
- **Path B — Bluetooth Classic:** firmware in **`bridge/`** on **ESP32 (NodeMCU)** + **`lolin_s3_mini_1_28_uart_bridge`** or **`lolin_s3_mini_1_28_wifi_bridge`** on the S3  
- **Bench / UI test:** `bridge` env **`esp32_bridge_sim`** (or **`_sim_wifi`**) — no OBD; fake RPM/speed/temp/voltage  

## Hardware (setup in use)

| Component | Description |
|-----------|-------------|
| **HUD display** | **Waveshare ESP32-S3-Touch-LCD-1.28** — 1.28″ round **240×240** IPS, capacitive touch, **ESP32-S3**, microSD. PlatformIO: `lolin_s3_mini_1_28` / default **`lolin_s3_mini_1_28_uart_bridge`**. |
| **OBD adapter** | ELM327-class device (e.g. **Bluetooth Classic** / BT3.0 dongle). Configure name/MAC in **`bridge/platformio.ini`** for the bridge build. |
| **Bridge (Classic BT)** | **ESP32 NodeMCU** (or similar ESP32, not S3) running **`bridge/`** — connects to the dongle over BT Classic, sends normalized `41 …` lines to the S3 on **UART** (default pins in `bridge/platformio.ini`: RX **16**, TX **17**) or **WiFi** to the HUD SoftAP. |

**Wiring (UART bridge):** connect **S3 UART** to **bridge UART** (cross TX/RX), common GND, 3V3 logic. Exact S3 pins: **`OBD_BRIDGE_RX_PIN` / `OBD_BRIDGE_TX_PIN`** in root `platformio.ini` (default RX **16**, TX **15** on S3 — verify against your board doc and re-flash if you change pins).

**Note:** ESP32-S3 does **not** speak Bluetooth **Classic**; a separate ESP32 bridge is required for BT3.0 ELM adapters.

## Parameters (dashboard)

- Engine RPM  
- Vehicle speed (km/h)  
- Coolant temperature  
- Battery / control-module voltage  
- OBD link status  

Protocol on the wire: **ISO 15765-4 CAN** between vehicle and adapter; HUD sees **ELM-style ASCII** responses (`41 0C …`, `41 0D …`, etc.).

## UI

Screens are defined under `lib/hud_ui/` (XML + generated C).

| Screen | Content |
|--------|---------|
| **Boot** | **Mazda** logo — asset `mazda_logo_small` (`lib/hud_ui/images/mazda_logo_small.png` → compiled image), centered on **`boot.xml` / `boot_gen.c`**. |
| **Dashboard** | Round HUD: RPM arc, speed center, voltage / coolant sides, bottom link strip (`dashboard_gen.c`). *Replace the screenshot below with your own in-car photo when you have one.* |
| **Settings** | Device / tuning screens as in repo. |

| Boot (Mazda logo) | Dashboard *(placeholder — swap for real install photo)* | Settings |
|-------------------|-----------------------------------------------------------|----------|
| ![Boot](lib/hud_ui/screenshots/boot.png?raw=true "Boot — Mazda logo") | ![Dashboard](lib/hud_ui/screenshots/dashboard.png?raw=true "Dashboard") | ![Settings](lib/hud_ui/screenshots/settings.png?raw=true "Settings") |

If `lib/hud_ui/screenshots/` is empty locally, add PNGs there or update the paths above after you export captures from hardware.

## Build (quick reference)

```text
# HUD (default env: UART bridge input)
pio run -t upload

# Bridge — OBD + UART to S3
cd bridge && pio run -e esp32_bridge_bt3 -t upload

# Bridge — UI demo without OBD
cd bridge && pio run -e esp32_bridge_sim -t upload
```

See **HANDOFF.md** for WiFi bridge envs, sim WiFi, brownout notes on NodeMCU, and protocol details.
