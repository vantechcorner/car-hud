#include <Arduino.h>
#include <BluetoothSerial.h>
#if defined(HUD_USE_WIFI)
#include <WiFi.h>
#include <WiFiUdp.h>
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

BluetoothSerial SerialBT;
#if !defined(HUD_USE_WIFI)
HardwareSerial HudSerial(2);
#endif

static String lineBuffer;
static bool obdReady = false;
static uint32_t lastAltPoll = 0;
static uint32_t lastSlow = 0;
static uint32_t lastReconnectTry = 0;
static bool pollSpeedNext = true; // xen kẽ 010D / 010C cho cảm giác “real-time”
static bool pollSlowTempNext = true; // xen kẽ 0105 / 0142 (một lệnh mỗi chu kỳ chậm)
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

void sendElm(const char *cmd)
{
  SerialBT.print(cmd);
  SerialBT.print("\r");
}

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

  if (s.length() >= 2 && s.startsWith("41"))
  {
    // Supported payloads:
    // - speed/temp: 41 PP XX   => compact length 6
    // - rpm / voltage: 41 PP AABB => compact length 8
    String normalized;
    if (s.length() == 6)
    {
      String pid = s.substring(2, 4);
      String v1 = s.substring(4, 6);
      normalized = "41 " + pid + " " + v1;
    }
    else if (s.length() == 8)
    {
      String pid = s.substring(2, 4);
      String v1 = s.substring(4, 6);
      String v2 = s.substring(6, 8);
      normalized = "41 " + pid + " " + v1 + " " + v2;
    }
    else
    {
#ifdef BRIDGE_DEBUG
      Serial.printf("[bridge] skip 41 len=%u line=%s compact=%s\n",
                    (unsigned)s.length(), line.c_str(), s.c_str());
#endif
      return;
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

void setup()
{
  Serial.begin(115200);
  delay(300); // USB-UART stable; avoids truncated first lines

  // BT Classic controller before WiFi reduces coexistence panics on many ESP32 boards.
  if (!SerialBT.begin("CarHUD-Bridge", true))
  {
    Serial.println("Bluetooth init failed");
  }

#if defined(HUD_USE_WIFI)
  delay(100);
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
    if (pollSlowTempNext)
      sendElm("0105"); // coolant temp
    else
      sendElm("0142"); // control module voltage
    pollSlowTempNext = !pollSlowTempNext;
  }

  delay(5);
}
