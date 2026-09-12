/*
 * XIAO RP2040 virtual finger for ZMK sleep_xiao / awake_xiao.
 *
 * D0: every LOAD_INTERVAL_MS → page-turn load (RIGHT on nRF)
 * D1: every STATUS_INTERVAL_MS → status trigger (soak_status types time/power)
 *
 * Wiring: nRF D0↔RP2040 D0, nRF D1↔RP2040 D1, GND shared.
 * USB on RP2040, LiPo on nRF. Do not tie 3V3/5V/BAT.
 */

constexpr uint8_t PIN_LOAD = D0;
constexpr uint8_t PIN_STATUS = D1;
constexpr uint32_t LOAD_INTERVAL_MS = 30000;
constexpr uint32_t STATUS_INTERVAL_MS = 300000; // 5 minutes
constexpr uint32_t PULSE_MS = 80;

static uint32_t last_load_ms = 0;
static uint32_t last_status_ms = 0;

static void pulsePinToGnd(uint8_t pin) {
  pinMode(PIN_LED_R, OUTPUT);
  digitalWrite(PIN_LED_R, LOW);

  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(PULSE_MS);
  pinMode(pin, INPUT);

  digitalWrite(PIN_LED_R, HIGH);
  pinMode(PIN_LED_R, INPUT);
}

void setup() {
  pinMode(PIN_LOAD, INPUT);
  pinMode(PIN_STATUS, INPUT);
  last_load_ms = millis();
  last_status_ms = millis();
  // First status shortly after boot so bring-up is obvious.
  if (STATUS_INTERVAL_MS > 15000) {
    last_status_ms = millis() - (STATUS_INTERVAL_MS - 15000);
  }
}

void loop() {
  const uint32_t now = millis();

  if ((now - last_load_ms) >= LOAD_INTERVAL_MS) {
    last_load_ms = now;
    pulsePinToGnd(PIN_LOAD);
  }

  if ((now - last_status_ms) >= STATUS_INTERVAL_MS) {
    last_status_ms = now;
    pulsePinToGnd(PIN_STATUS);
  }
}
