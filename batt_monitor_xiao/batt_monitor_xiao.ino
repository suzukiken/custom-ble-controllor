/*
 * XIAO nRF52840 battery telemetry (no sleep).
 *
 * Exposes TWO ways to read the same line (macOS often caches GATT after reflash):
 *  1) Custom char (read+notify): 7f5f0002-7a4b-4c8f-9e2d-1b3c5a7e9f01
 *  2) Nordic UART TX notify:     6e400003-b5a3-f393-e0a9-e50e24dcca9e
 *
 * Payload: uptime_s=<sec> percent=<0-100> voltage_mv=<mv>\n
 *
 * After reflash on macOS: toggle Bluetooth off/on (or Forget BattMon) so GATT
 * cache refreshes — otherwise the Mac may still show only the old NUS table.
 *
 * LEDs: boot R→G→B self-test only; red flash on each sample. No heartbeat LEDs.
 */

#include <Adafruit_TinyUSB.h>
#include <bluefruit.h>

constexpr uint32_t INTERVAL_MS = 5UL * 60UL * 1000UL; // 5 minutes
constexpr char DEVICE_NAME[] = "BattMon Xiao";

#ifndef PIN_VBAT
#define PIN_VBAT 32
#endif
#ifndef PIN_VBAT_ENABLE
#define PIN_VBAT_ENABLE VBAT_ENABLE
#endif

BLEService battService("7f5f0001-7a4b-4c8f-9e2d-1b3c5a7e9f01");
BLECharacteristic battChar("7f5f0002-7a4b-4c8f-9e2d-1b3c5a7e9f01");
BLEUart bleuart;

static uint32_t g_boot_ms = 0;
static uint32_t g_last_send_ms = 0;
static char g_last_line[96] = "uptime_s=0 percent=0 voltage_mv=0\n";

static float readBatteryVolts() {
  digitalWrite(PIN_VBAT_ENABLE, LOW);
  delay(2);
  uint32_t sum = 0;
  for (int i = 0; i < 4; i++) {
    sum += analogRead(PIN_VBAT);
    delay(1);
  }
  return 2.961f * 3.6f * (sum / 4.0f) / 4096.0f;
}

static uint8_t percentFromMv(uint16_t mv) {
  static const struct {
    uint16_t mv;
    uint8_t pct;
  } kTable[] = {
      {4200, 100}, {4130, 95}, {4060, 90}, {3980, 80}, {3900, 70},
      {3820, 60},  {3750, 50}, {3680, 40}, {3620, 30}, {3550, 20},
      {3450, 10},  {3350, 5},  {3000, 0},
  };
  if (mv >= kTable[0].mv) {
    return 100;
  }
  const size_t n = sizeof(kTable) / sizeof(kTable[0]);
  for (size_t i = 1; i < n; i++) {
    if (mv >= kTable[i].mv) {
      const uint16_t hi = kTable[i - 1].mv;
      const uint16_t lo = kTable[i].mv;
      const uint8_t phi = kTable[i - 1].pct;
      const uint8_t plo = kTable[i].pct;
      if (hi == lo) {
        return plo;
      }
      return plo + (uint8_t)(((uint32_t)(mv - lo) * (phi - plo)) / (hi - lo));
    }
  }
  return 0;
}

static void sampleToBuffer() {
  const uint32_t uptime_s = (millis() - g_boot_ms) / 1000UL;
  const float volts = readBatteryVolts();
  uint16_t mv = 0;
  if (volts > 0.5f && volts < 5.5f) {
    mv = (uint16_t)(volts * 1000.0f + 0.5f);
  }
  const uint8_t pct = percentFromMv(mv);
  snprintf(g_last_line, sizeof(g_last_line),
           "uptime_s=%lu percent=%u voltage_mv=%u\n",
           (unsigned long)uptime_s, (unsigned)pct, (unsigned)mv);
}

static void publishSample() {
  digitalWrite(LED_RED, LOW);

  sampleToBuffer();
  const uint16_t len = (uint16_t)strlen(g_last_line);

  battChar.write((const uint8_t *)g_last_line, len);
  if (Bluefruit.connected()) {
    battChar.notify((const uint8_t *)g_last_line, len);
    bleuart.write((const uint8_t *)g_last_line, len);
  }

  delay(80);
  digitalWrite(LED_RED, HIGH);
  g_last_send_ms = millis();
}

static void bleuart_rx_callback(uint16_t conn_hdl) {
  (void)conn_hdl;
  while (bleuart.available()) {
    (void)bleuart.read();
  }
  g_last_send_ms = 0; // request publish from loop (no heavy work here)
}

static void connect_callback(uint16_t conn_hdl) {
  (void)conn_hdl;
  g_last_send_ms = 0;
}

static void disconnect_callback(uint16_t conn_hdl, uint8_t reason) {
  (void)conn_hdl;
  (void)reason;
  Bluefruit.Advertising.start(0);
}

void setup() {
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);

  digitalWrite(LED_RED, LOW);
  delay(200);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, LOW);
  delay(200);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, LOW);
  delay(200);
  digitalWrite(LED_BLUE, HIGH);

  pinMode(PIN_VBAT_ENABLE, OUTPUT);
  digitalWrite(PIN_VBAT_ENABLE, LOW);
  pinMode(PIN_VBAT, INPUT);
  analogReference(AR_DEFAULT);
  analogReadResolution(12);

  g_boot_ms = millis();
  sampleToBuffer();

  Bluefruit.autoConnLed(false);
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName(DEVICE_NAME);
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // Custom readable telemetry
  battService.begin();
  battChar.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  battChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  battChar.setMaxLen(sizeof(g_last_line));
  battChar.begin();
  battChar.write((const uint8_t *)g_last_line, strlen(g_last_line));

  // Nordic UART (helps when Mac still has NUS cached from older builds)
  bleuart.begin();
  bleuart.setRxCallback(bleuart_rx_callback);

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addName();
  Bluefruit.ScanResponse.addService(battService);
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void loop() {
  const uint32_t now = millis();

  if (Bluefruit.connected()) {
    if (g_last_send_ms == 0 || (now - g_last_send_ms) >= INTERVAL_MS) {
      publishSample();
    }
  }

  delay(50);
}
