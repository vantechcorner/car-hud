#include <Arduino.h>
#include <stdio.h>
#if !defined(BRIDGE_SIMULATE)
#include <BluetoothSerial.h>
#endif
#if defined(HUD_USE_WIFI) || defined(BRIDGE_SIMULATE)
#include <WiFi.h>
#endif
#if defined(HUD_USE_WIFI)
#include <WiFiUdp.h>
#endif
#if !defined(HUD_USE_WIFI) && !defined(BRIDGE_SIMULATE)
/* UART-only OBD bridge: Wi-Fi off so BT Classic peak is not stacked (brownout). */
#include <WiFi.h>
#endif

// ---------- Bridge output to ESP32-S3 HUD ----------
#if defined(HUD_USE_WIFI)
#ifndef HUD_UDP_PORT
#define HUD_UDP_PORT 3333
#endif
// Default: send UDP to WiFi.gatewayIP() (HUD as SoftAP => 192.168.4.1).
WiFiUDP hudUdp;
#else
#ifndef HUD_UART_BAUD
#define HUD_UART_BAUD 115200
#endif
#ifndef HUD_UART_RX_PIN
#define HUD_UART_RX_PIN 16
#endif
#ifndef HUD_UART_TX_PIN
#define HUD_UART_TX_PIN 17
#endif
#endif

// ---------- OBD adapter connection ----------
// build_flags: OBD_BT_NAME (tried first), OBD_BT_MAC (fallback if name fails)
// -D OBD_BT_NAME=\"V-LINK\"
// -D OBD_BT_MAC=\"AA:BB:CC:DD:EE:FF\"
#ifndef OBD_BT_MAC
#define OBD_BT_MAC ""
#endif
#ifndef OBD_BT_NAME
#define OBD_BT_NAME "V-LINK"
#endif

#if !defined(BRIDGE_SIMULATE)
BluetoothSerial SerialBT;
#endif
#if !defined(HUD_USE_WIFI)
HardwareSerial HudSerial(2);
#endif

#if !defined(BRIDGE_SIMULATE)
static String lineBuffer;
static bool obdReady = false;
#endif
static uint32_t lastAltPoll = 0;
static uint32_t lastSlow = 0;
#if !defined(BRIDGE_SIMULATE)
static uint32_t lastReconnectTry = 0;
#endif
static bool pollSpeedNext = true; // xen kẽ 010D / 010C cho cảm giác “real-time”
#if defined(HUD_USE_WIFI)
static uint32_t lastWifiTry = 0;
#endif

// ELM327 half-duplex: one request at a time. Alternating speed/RPM avoids starving either PID.
#ifndef BRIDGE_ALTERNATE_POLL_MS
#define BRIDGE_ALTERNATE_POLL_MS 80U
#endif
#ifndef BRIDGE_TEMP_MS
#define BRIDGE_TEMP_MS 4000U
#endif

static const uint32_t ALT_POLL_INTERVAL = BRIDGE_ALTERNATE_POLL_MS;
static const uint32_t SLOW_INTERVAL = BRIDGE_TEMP_MS;

#if !defined(BRIDGE_SIMULATE)
void sendElm(const char *cmd)
{
  SerialBT.print(cmd);
  SerialBT.print("\r");
}
#endif

#if defined(HUD_UDP_USE_STATIC_TARGET)
static IPAddress hudUdpTarget(
    HUD_UDP_TARGET_OCT1, HUD_UDP_TARGET_OCT2, HUD_UDP_TARGET_OCT3, HUD_UDP_TARGET_OCT4);
#endif

void emitToHud(const String &s)
{
#if defined(HUD_USE_WIFI)
  if (WiFi.status() != WL_CONNECTED)
    return;
#if defined(HUD_UDP_USE_STATIC_TARGET)
  IPAddress target = hudUdpTarget;
#else
  IPAddress target = WiFi.gatewayIP();
  if ((uint32_t)target == 0)
    return;
#endif
  hudUdp.beginPacket(target, HUD_UDP_PORT);
  hudUdp.print(s);
  hudUdp.endPacket();
#else
  HudSerial.println(s);
#endif
}

#if !defined(BRIDGE_SIMULATE)
void processElmLine(String line)
{
  line.trim();
  line.toUpperCase();

  if (line.isEmpty())
    return;

  // Ignore command echo and status noise.
  if (line == "OK" || line.startsWith("AT") || line.startsWith("SEARCHING"))
    return;

  // Forward OBD mode 01 response.
  // Some ELM adapters respond as compact hex without spaces, e.g.:
  //   "41 0C 1A F8"  OR  "410C1AF8"
  // The S3 HUD parser currently expects spaced form.
  String s = line;
  s.replace(" ", "");

  // Some adapters include headers, e.g. "7E8..." or other prefixes before the "41" payload.
  // Try to locate the start of the mode-01 response anywhere in the line.
  int p = s.indexOf("41");
  if (p > 0)
    s = s.substring(p);

  if (s.length() >= 6 && s.startsWith("41"))
  {
    /* Many ELM lines are longer than 6/8 hex (padding, multi-frame tail). Only exact
     * length matched before — real cars often sent e.g. "41057B...." and we dropped it. */
    String pid = s.substring(2, 4);
    unsigned long ppl = strtoul(pid.c_str(), nullptr, 16);
    String normalized;
    if (ppl == 0x0C || ppl == 0x42)
    {
      if (s.length() < 8)
        return;
      String v1 = s.substring(4, 6);
      String v2 = s.substring(6, 8);
      normalized = "41 " + pid + " " + v1 + " " + v2;
    }
    else
    {
      String v1 = s.substring(4, 6);
      normalized = "41 " + pid + " " + v1;
    }
    emitToHud(normalized);
#ifdef BRIDGE_DEBUG
    Serial.printf("[bridge] ok line=%s -> hud=%s\n", line.c_str(), normalized.c_str());
#endif
  }
}

void processElmIncoming()
{
  while (SerialBT.available())
  {
    char c = (char)SerialBT.read();
    if (c == '\r' || c == '\n' || c == '>')
    {
      if (!lineBuffer.isEmpty())
      {
        processElmLine(lineBuffer);
        lineBuffer = "";
      }
      continue;
    }

    if ((uint8_t)c < 32 || (uint8_t)c > 126)
      continue;

    lineBuffer += c;
    if (lineBuffer.length() > 128)
    {
      lineBuffer = "";
    }
  }
}

bool connectObdAdapter()
{
  // Name first (usually faster when it works); MAC only if name fails.
  if (SerialBT.connect(OBD_BT_NAME))
    return true;
  if (strlen(OBD_BT_MAC) > 0)
    return SerialBT.connect(OBD_BT_MAC);
  return false;
}

void initElm()
{
  sendElm("ATZ");
  delay(800);
  sendElm("ATE0");
  delay(250);
  sendElm("ATL0");
  delay(250);
  sendElm("ATS0");
  delay(250);
  sendElm("ATSP6");
  delay(300);
  obdReady = true;
  lastAltPoll = lastSlow = millis();
  pollSpeedNext = true;
}

#endif /* !BRIDGE_SIMULATE */

#if defined(BRIDGE_SIMULATE)

#ifndef SIM_CYCLE_MS
#define SIM_CYCLE_MS 40000U
#endif
#ifndef SIM_RPM_IDLE
#define SIM_RPM_IDLE 720U
#endif
#ifndef SIM_RPM_MAX
#define SIM_RPM_MAX 5200U
#endif

static float sim_smoothstep(float t)
{
  if (t <= 0.f)
    return 0.f;
  if (t >= 1.f)
    return 1.f;
  return t * t * (3.f - 2.f * t);
}

static uint16_t sim_rpm_at(uint32_t ms)
{
  const uint32_t c = SIM_CYCLE_MS;
  const uint32_t T_UP = c * 30 / 100;
  const uint32_t T_HOLD = c * 45 / 100;
  const uint32_t T_DOWN = c * 78 / 100;
  uint32_t t = ms % c;
  const uint16_t lo = SIM_RPM_IDLE;
  const uint16_t hi = SIM_RPM_MAX;

  if (t < T_UP)
  {
    float u = sim_smoothstep((float)t / (float)T_UP);
    return lo + (uint16_t)(u * (float)(hi - lo));
  }
  if (t < T_HOLD)
    return hi;
  if (t < T_DOWN)
  {
    float u = sim_smoothstep((float)(t - T_HOLD) / (float)(T_DOWN - T_HOLD));
    return hi - (uint16_t)(u * (float)(hi - lo));
  }
  return lo;
}

static uint8_t sim_kmh_at(uint32_t ms)
{
  uint16_t rpm = sim_rpm_at(ms);
  if (rpm <= 850)
    return 0;
  uint32_t v = (uint32_t)(rpm - 800) * 125U / (unsigned)(SIM_RPM_MAX - 800);
  if (v > 220)
    v = 220;
  return (uint8_t)v;
}

static int16_t sim_coolant_at(uint32_t ms)
{
  uint16_t rpm = sim_rpm_at(ms);
  int span = (int)SIM_RPM_MAX - (int)SIM_RPM_IDLE;
  int base = 72 + (int)((rpm - SIM_RPM_IDLE) * 22 / (span > 1 ? span : 1));
  if (base < 72)
    base = 72;
  if (base > 96)
    base = 96;
  return (int16_t)base;
}

static void sim_emit_0d(uint8_t kmh)
{
  char b[20];
  snprintf(b, sizeof(b), "41 0D %02X", (unsigned)kmh);
  emitToHud(String(b));
}

static void sim_emit_0c(uint16_t rpm)
{
  uint32_t raw = (uint32_t)rpm * 4U;
  if (raw > 0xFFFFu)
    raw = 0xFFFFu;
  unsigned hi = (raw >> 8) & 0xFF;
  unsigned lo = raw & 0xFF;
  char b[24];
  snprintf(b, sizeof(b), "41 0C %02X %02X", hi, lo);
  emitToHud(String(b));
}

static void sim_emit_05_c(int16_t temp_c)
{
  int a = (int)temp_c + 40;
  if (a < 0)
    a = 0;
  if (a > 255)
    a = 255;
  char b[20];
  snprintf(b, sizeof(b), "41 05 %02X", (unsigned)a);
  emitToHud(String(b));
}

static void sim_emit_42_mv(uint16_t mv)
{
  unsigned hi = (mv >> 8) & 0xFF;
  unsigned lo = mv & 0xFF;
  char b[24];
  snprintf(b, sizeof(b), "41 42 %02X %02X", hi, lo);
  emitToHud(String(b));
}

void setup()
{
  Serial.begin(115200);
  delay(200);
#if defined(HUD_USE_WIFI)
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  Serial.println("Car HUD bridge SIMULATOR (WiFi -> HUD UDP)");
  Serial.printf("Join AP SSID=%s port=%u\n", WIFI_STA_SSID, (unsigned)HUD_UDP_PORT);
  WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);
#else
  WiFi.mode(WIFI_OFF);
  HudSerial.begin(HUD_UART_BAUD, SERIAL_8N1, HUD_UART_RX_PIN, HUD_UART_TX_PIN);
  Serial.println("Car HUD bridge SIMULATOR (UART -> HUD, no BT/OBD)");
  Serial.printf("HUD UART TX=%d RX=%d baud=%u\n", HUD_UART_TX_PIN, HUD_UART_RX_PIN,
                (unsigned)HUD_UART_BAUD);
#endif
  lastAltPoll = lastSlow = millis();
  pollSpeedNext = true;
}

void loop()
{
#if defined(HUD_USE_WIFI)
  if (WiFi.status() != WL_CONNECTED)
  {
    uint32_t nw = millis();
    if (nw - lastWifiTry > 3000)
    {
      lastWifiTry = nw;
      Serial.println("WiFi (sim) reconnecting...");
      WiFi.disconnect(true, false);
      WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);
    }
  }
#endif

  uint32_t now = millis();

  if (now - lastAltPoll >= ALT_POLL_INTERVAL)
  {
    lastAltPoll = now;
    uint16_t r = sim_rpm_at(now);
    uint8_t k = sim_kmh_at(now);
    if (pollSpeedNext)
      sim_emit_0d(k);
    else
      sim_emit_0c(r);
    pollSpeedNext = !pollSpeedNext;
  }

  if (now - lastSlow >= SLOW_INTERVAL)
  {
    lastSlow = now;
    sim_emit_05_c(sim_coolant_at(now));
    uint16_t mv = (uint16_t)(13700u + (unsigned)sim_rpm_at(now) / 200U);
    if (mv > 14500)
      mv = 14500;
    sim_emit_42_mv(mv);
  }

  delay(5);
}

#else

void setup()
{
  Serial.begin(115200);
  delay(400); /* USB enumerate + bulk cap charge before radio */

#if !defined(HUD_USE_WIFI)
  WiFi.mode(WIFI_OFF);
#endif

  // BT Classic before WiFi on dual-radio builds reduces coexistence panics.
  if (!SerialBT.begin("CarHUD-Bridge", true))
  {
    Serial.println("Bluetooth init failed");
  }

#if defined(HUD_USE_WIFI)
  delay(350); /* avoid BT + Wi-Fi scan peak overlapping on weak 5 V */
  WiFi.mode(WIFI_STA);
  // Modem sleep can add 100–300ms+ jitter on STA → HUD; HUD path needs low latency.
  WiFi.setSleep(false);
  Serial.println("Car HUD BT3.0 bridge (WiFi -> S3 HUD)");
  Serial.printf("Joining AP SSID=%s for UDP port %u\n", WIFI_STA_SSID, (unsigned)HUD_UDP_PORT);
  WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);
#else
  HudSerial.begin(HUD_UART_BAUD, SERIAL_8N1, HUD_UART_RX_PIN, HUD_UART_TX_PIN);
  Serial.println("Car HUD BT3.0 bridge starting...");
  Serial.printf("HUD UART TX=%d RX=%d baud=%d\n", HUD_UART_TX_PIN, HUD_UART_RX_PIN, HUD_UART_BAUD);
#endif
}

void loop()
{
#if defined(HUD_USE_WIFI)
  if (WiFi.status() != WL_CONNECTED)
  {
    uint32_t nw = millis();
    if (nw - lastWifiTry > 3000)
    {
      lastWifiTry = nw;
      Serial.println("WiFi reconnecting...");
      WiFi.disconnect(true, false);
      WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);
    }
  }
#endif

  if (!SerialBT.connected())
  {
    obdReady = false;
    uint32_t now = millis();
    if (now - lastReconnectTry > 2500)
    {
      lastReconnectTry = now;
      if (connectObdAdapter())
      {
        Serial.println("OBD connected");
        initElm();
      }
      else
      {
        Serial.println("OBD connect retry...");
      }
    }
    delay(20);
    return;
  }

  processElmIncoming();

  if (!obdReady)
  {
    initElm();
    return;
  }

  uint32_t now = millis();

  if (now - lastAltPoll >= ALT_POLL_INTERVAL)
  {
    lastAltPoll = now;
    if (pollSpeedNext)
      sendElm("010D"); // speed
    else
      sendElm("010C"); // RPM
    pollSpeedNext = !pollSpeedNext;
  }

  if (now - lastSlow >= SLOW_INTERVAL)
  {
    lastSlow = now;
    sendElm("0105"); // coolant temp
    delay(40);
    sendElm("0142"); // control module voltage (many ECUs; skip if unsupported)
  }

  delay(5);
}

#endif /* BRIDGE_SIMULATE */
