/*
 * XIAO nRF52840 battery telemetry (no sleep).
 *
 * Every INTERVAL_MS (and on BLE connect), send via Nordic UART Service:
 *   uptime_s=<sec> percent=<0-100> voltage_mv=<mv>\n
 *
 * Board package: Seeed nRF52 Boards → "Seeed XIAO nRF52840"
 * (Adafruit Bluefruit). Flash as UF2 (double-reset → XIAO-SENSE / XIAO BLE).
 *
 * Power from LiPo on BAT+. Keep USB unplugged during long runs if you want
 * true battery drain; USB present will charge and skew voltage.
 */

#include <Adafruit_TinyUSB.h>
#include <bluefruit.h>

constexpr uint32_t INTERVAL_MS = 10UL * 60UL * 1000UL; // 10 minutes
constexpr char DEVICE_NAME[] = "BattMon Xiao";

// XIAO nRF52840 battery sense (Seeed / forum convention)
#ifndef PIN_VBAT
#define PIN_VBAT 32
#endif
#ifndef PIN_VBAT_ENABLE
#define PIN_VBAT_ENABLE VBAT_ENABLE // 14 on Seeed XIAO nRF52840
#endif

BLEUart bleuart;

static uint32_t g_boot_ms = 0;
static uint32_t g_last_send_ms = 0;
static bool g_sent_this_conn = false;

static float readBatteryVolts() {
  // Divider always enabled (safe while measuring / charging per Seeed Q3).
  digitalWrite(PIN_VBAT_ENABLE, LOW);
  delay(5);

  uint32_t sum = 0;
  constexpr int SAMPLES = 8;
  for (int i = 0; i < SAMPLES; i++) {
    sum += analogRead(PIN_VBAT);
    delay(2);
  }
  const float adc = sum / float(SAMPLES);
  // msfujino: ratio ~2.961, Vref 3.6V, 12-bit ADC
  return 2.961f * 3.6f * adc / 4096.0f;
}

static uint8_t percentFromMv(uint16_t mv) {
  // Coarse 1S LiPo open-circuit-ish curve (not coulomb counting).
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
      const uint16_t mv_hi = kTable[i - 1].mv;
      const uint16_t mv_lo = kTable[i].mv;
      const uint8_t pct_hi = kTable[i - 1].pct;
      const uint8_t pct_lo = kTable[i].pct;
      const uint32_t span = mv_hi - mv_lo;
      if (span == 0) {
        return pct_lo;
      }
      return pct_lo +
             (uint8_t)(((uint32_t)(mv - mv_lo) * (pct_hi - pct_lo)) / span);
    }
  }
  return 0;
}

static void sendTelemetry() {
  const uint32_t uptime_s = (millis() - g_boot_ms) / 1000UL;
  const float volts = readBatteryVolts();
  uint16_t mv = 0;
  if (volts > 0.0f) {
    mv = (uint16_t)(volts * 1000.0f + 0.5f);
  }
  const uint8_t pct = percentFromMv(mv);

  char line[96];
  snprintf(line, sizeof(line),
           "uptime_s=%lu percent=%u voltage_mv=%u\n",
           (unsigned long)uptime_s, (unsigned)pct, (unsigned)mv);

  if (Bluefruit.connected()) {
    bleuart.write((const uint8_t *)line, strlen(line));
  }

  // Red LED flash = sample sent (active-low on XIAO)
  digitalWrite(LED_RED, LOW);
  delay(40);
  digitalWrite(LED_RED, HIGH);

  g_last_send_ms = millis();
}

static void connect_callback(uint16_t conn_hdl) {
  (void)conn_hdl;
  // Do not send here — Mac has not enabled notifications yet.
  g_sent_this_conn = false;
}

static void disconnect_callback(uint16_t conn_hdl, uint8_t reason) {
  (void)conn_hdl;
  (void)reason;
  g_sent_this_conn = false;
  Bluefruit.Advertising.start(0);
}

void setup() {
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_RED, HIGH);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_BLUE, HIGH);

  pinMode(PIN_VBAT_ENABLE, OUTPUT);
  digitalWrite(PIN_VBAT_ENABLE, LOW);
  pinMode(PIN_VBAT, INPUT);

  analogReference(AR_DEFAULT); // 0.6V * 6 = 3.6V
  analogReadResolution(12);

  g_boot_ms = millis();

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName(DEVICE_NAME);
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  bleuart.begin();

  // Put the name in the primary ADV packet so macOS/bleak sees it without
  // relying on scan-response timing. NUS UUID goes in the scan response.
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addName();
  Bluefruit.ScanResponse.addService(bleuart);
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244); // 20ms .. 152.5ms
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);

  // Slow blue blink while advertising (stops when connected / sample flash).
  digitalWrite(LED_BLUE, LOW);
  delay(80);
  digitalWrite(LED_BLUE, HIGH);

  g_last_send_ms = millis();
}

void loop() {
  // No System OFF / no deep sleep — SoftDevice stays up for BLE.
  if (Bluefruit.connected()) {
    // Wait until the host enables NUS notifications, then send immediately
    // and every INTERVAL_MS after that.
    if (bleuart.notifyEnabled()) {
      const uint32_t now = millis();
      if (!g_sent_this_conn || (now - g_last_send_ms) >= INTERVAL_MS) {
        sendTelemetry();
        g_sent_this_conn = true;
      }
    }
  } else {
    // Heartbeat: advertising alive
    static uint32_t last_hb = 0;
    const uint32_t now = millis();
    if (now - last_hb >= 2000) {
      last_hb = now;
      digitalWrite(LED_BLUE, LOW);
      delay(30);
      digitalWrite(LED_BLUE, HIGH);
    }
  }
  delay(100);
}
