# Car HUD — handoff for Cursor / future sessions

**Last verified (user):** ESP32-S3 round 240×240 UI + ESP32 NodeMCU bridge **sim** profile work end-to-end. Use this file to resume without re-discovering architecture.

---

## Architecture

| Role | Hardware | Firmware | Talks to HUD |
|------|-----------|----------|--------------|
| **HUD** | ESP32-S3 + 1.28″ 240×240 (e.g. Waveshare S3 Touch LCD 1.28) | Root `platformio.ini` → env `lolin_s3_mini_1_28_uart_bridge` (default) or `*_wifi_bridge` | Receives ELM-style ASCII lines |
| **Bridge** | ESP32 NodeMCU (or similar) | `bridge/` PlatformIO | BT Classic → ELM327, forwards normalized `41 …` lines |

**Data path (production):** OBD dongle ↔ Bridge (BT) ↔ **UART** or **UDP** ↔ S3 `parseObd()` → LVGL subjects (`engine_rpm`, `speed`, `coolant_temp`, `battery_tenths`, `con_error`).

**Data path (bench):** Bridge build **`esp32_bridge_sim`** (UART) or **`esp32_bridge_sim_wifi`** — no Bluetooth/OBD; synthesizes same `41 0D` / `41 0C` / `41 05` / `41 42` lines.

---

## Key source files

| Area | Path | Notes |
|------|------|--------|
| HUD OBD parse + BLE / UART / WiFi bridge | `src/main.cpp` | `parseObd()` strips hex, expects `41` mode-01; PIDs 0x0C, 0x0D, 0x05, 0x42 |
| Round dashboard UI | `lib/hud_ui/screens/dashboard_gen.c` | RPM arc, speed block, Volt/Temp sides, OBD strip |
| Subjects init | `lib/hud_ui/hud_ui_gen.c` | `battery_tenths` init `-1` → `-- V`; `coolant_temp` init `-128` → `--°C` until PID 05 |
| Real bridge + sim | `bridge/src/main.cpp` | `#ifdef BRIDGE_SIMULATE` — sim cycle ~40 s ramp/hold/decay RPM & speed |
| Bridge partitions | `bridge/partitions_bridge.csv` | Avoid `huge_app` on 2 MB flash (was causing boot issues) |
| Bridge PlatformIO | `bridge/platformio.ini` | Envs below |

---

## PlatformIO environments (bridge)

- **`esp32_bridge_bt3`** — Production: BT to OBD, UART to S3 (`HUD_UART_RX_PIN` 16, `HUD_UART_TX_PIN` 17 by default in `bridge/platformio.ini`).
- **`esp32_bridge_bt3_wifi`** — BT + WiFi STA → UDP to HUD (`HUD_UDP_PORT` 3333, SSID/pass must match S3 SoftAP if used).
- **`esp32_bridge_sim`** — **No BT/OBD**; UART only; fake vehicle data for UI testing.
- **`esp32_bridge_sim_wifi`** — Same sim over WiFi UDP.

**Brownout on NodeMCU (real bridge):** Mitigations in firmware: `WiFi.mode(WIFI_OFF)` on UART build before BT, delays, `board_build.f_cpu = 160MHz`, `CONFIG_ESP32_BROWNOUT_DET_LVL_SEL_0`. Hardware: solid USB / cap on 3V3 if still brownout.

---

## PlatformIO environments (S3 HUD)

- Default: **`lolin_s3_mini_1_28_uart_bridge`** — `OBD_UART_BRIDGE`, pins `OBD_BRIDGE_RX_PIN` / `OBD_BRIDGE_TX_PIN` (see root `platformio.ini`; must match wiring to NodeMCU UART).
- **`lolin_s3_mini_1_28_wifi_bridge`** — `OBD_WIFI_BRIDGE`, UDP port aligned with bridge WiFi build.

---

## Wire protocol (HUD side)

Bridge must emit **spaced** hex (HUD parser is line-based ASCII), e.g.:

- Speed: `41 0D XX` (km/h)
- RPM: `41 0C AA BB` with RPM = `((A<<8)|B)/4`
- Coolant: `41 05 XX` with °C = `A - 40`
- Voltage: `41 42 AA BB` with mV = `(A<<8)|B`, HUD stores tenths-of-volt

**Bridge `processElmLine`:** Strips spaces, finds `41`, normalizes 6-char (1-byte PID) or 8-char (0x0C, 0x42) from **prefix** of compact hex so padded ELM lines are not dropped.

**Slow PIDs:** Real bridge and direct-BLE HUD path send **both** `0105` and `0142` each slow interval (not strict alternation) so temp/volt are not starved.

---

## UI decisions (dashboard)

- Speed digits: **fixed label width** from `lv_text_get_size("888", …)` + `LV_LABEL_LONG_MODE_CLIP` + `LV_TEXT_ALIGN_CENTER` so multi-digit speed stays visually centered (bind_text alone kept left edge fixed).
- RPM label / `km/h`: slight transform scale (`DASH_RPM_KMH_SMALL`); gap RPM → speed via `align_to(speed_val, rpm_tag, OUT_BOTTOM_MID, DASH_RPM_TO_SPEED_GAP)`.
- OBD status text: slightly smaller (`DASH_LINK_SMALL`).
- Coolant: observer sets `--°C` when value `< -100` (sentinel `-128`).

---

## User-specific config to re-check

- Bridge `OBD_BT_MAC` / `OBD_BT_NAME` in `bridge/platformio.ini`.
- S3 `OBD_BRIDGE_*` pins vs actual wiring (S3 TX → bridge RX, etc.).
- WiFi SSID/password if using WiFi bridge builds.

---

## Build / flash (reference)

```text
# S3 HUD (default env in repo)
pio run -t upload

# Bridge — production UART
cd bridge && pio run -e esp32_bridge_bt3 -t upload

# Bridge — bench sim UART
cd bridge && pio run -e esp32_bridge_sim -t upload
```

---

## Open / future ideas (not blocking)

- Optional core dump partition on bridge to silence `No core dump partition` log (cosmetic).
- If PID `0x42` unsupported on some cars, voltage may stay `-- V`; PID `0x42` formula already matches SAE in `parseObd`.
- Sim: tune `SIM_CYCLE_MS`, `SIM_RPM_IDLE`, `SIM_RPM_MAX` via `-D` in `bridge/platformio.ini` if needed.

---

*End of handoff.*
