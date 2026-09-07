/*
 * XIAO nRF52840 battery telemetry (no sleep).
 *
 * While connected: send every INTERVAL_MS via Nordic UART:
 *   uptime_s=<sec> percent=<0-100> voltage_mv=<mv>\n
 * Host write to NUS RX also requests an immediate sample (flag only in callback).
 *
 * LEDs (active low): blue=advertising, green=connected heartbeat, red=send.
 *
 * Board: Seeed nRF52 Boards → Seeed XIAO nRF52840. Flash as UF2.
 */

#include <Adafruit_TinyUSB.h>
#include <bluefruit.h>

constexpr uint32_t INTERVAL_MS = 30UL * 1000UL;
constexpr char DEVICE_NAME[] = "BattMon Xiao";

#ifndef PIN_VBAT
#define PIN_VBAT 32
#endif
#ifndef PIN_VBAT_ENABLE
#define PIN_VBAT_ENABLE VBAT_ENABLE
#endif

BLEUart bleuart;

static uint32_t g_boot_ms = 0;
static uint32_t g_last_send_ms = 0;
static volatile bool g_connected = false;
static volatile bool g_poll_requested = false;

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

static void sendTelemetry() {
  // Red ON for the whole send attempt (visible even if we hang in ADC).
  digitalWrite(LED_RED, LOW);

  const uint32_t uptime_s = (millis() - g_boot_ms) / 1000UL;
  float volts = 0.0f;
  volts = readBatteryVolts();
  uint16_t mv = 0;
  if (volts > 0.5f && volts < 5.5f) {
    mv = (uint16_t)(volts * 1000.0f + 0.5f);
  }
  const uint8_t pct = percentFromMv(mv);

  char line[96];
  snprintf(line, sizeof(line),
           "uptime_s=%lu percent=%u voltage_mv=%u\n",
           (unsigned long)uptime_s, (unsigned)pct, (unsigned)mv);

  // Flush notify several ways — some hosts need notifyEnabled.
  if (Bluefruit.connected()) {
    bleuart.print(line);
    bleuart.flush();
  }

  delay(100);
  digitalWrite(LED_RED, HIGH);
  g_last_send_ms = millis();
}

static void bleuart_rx_callback(uint16_t conn_hdl) {
  (void)conn_hdl;
  while (bleuart.available()) {
    (void)bleuart.read();
  }
  g_poll_requested = true;
}

static void connect_callback(uint16_t conn_hdl) {
  (void)conn_hdl;
  g_connected = true;
  g_poll_requested = true; // send ASAP from loop after connect
  g_last_send_ms = 0;
}

static void disconnect_callback(uint16_t conn_hdl, uint8_t reason) {
  (void)conn_hdl;
  (void)reason;
  g_connected = false;
  g_poll_requested = false;
  Bluefruit.Advertising.start(0);
}

void setup() {
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);

  // Boot LED self-test: R → G → B (confirms which UF2 is running).
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

  Bluefruit.autoConnLed(false);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName(DEVICE_NAME);
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  bleuart.begin();
  bleuart.setRxCallback(bleuart_rx_callback);

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addName();
  Bluefruit.ScanResponse.addService(bleuart);
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void loop() {
  const uint32_t now = millis();

  // Prefer SoftDevice connection state; keep flag in sync.
  const bool connected = Bluefruit.connected();
  if (!connected) {
    g_connected = false;
  }

  if (connected) {
    static uint32_t last_hb = 0;
    if (now - last_hb >= 1000) {
      last_hb = now;
      digitalWrite(LED_GREEN, LOW);
      delay(25);
      digitalWrite(LED_GREEN, HIGH);
    }

    // Send: on poll/connect request, or every INTERVAL_MS.
    // g_last_send_ms==0 means "never sent this boot/connection".
    const bool interval_due =
        (g_last_send_ms == 0) || ((now - g_last_send_ms) >= INTERVAL_MS);
    if (g_poll_requested || interval_due) {
      g_poll_requested = false;
      sendTelemetry();
    }
  } else {
    static uint32_t last_adv = 0;
    if (now - last_adv >= 2000) {
      last_adv = now;
      digitalWrite(LED_BLUE, LOW);
      delay(30);
      digitalWrite(LED_BLUE, HIGH);
    }
  }

  delay(20);
}
