/*
 * XIAO RP2040 virtual finger for ZMK sleep_xiao / awake_xiao.
 *
 * D0 every INTERVAL_MS → status trigger (soak_status types time/power/mode).
 *
 * Wiring: nRF D0↔RP2040 D0, GND shared.
 * USB on RP2040, LiPo on nRF. Do not tie 3V3/5V/BAT.
 *
 * PULSE_MS must stay LOW long enough for nRF deep-sleep wake + kscan debounce.
 */

constexpr uint8_t PIN_STATUS = D0;
constexpr uint32_t INTERVAL_MS = 300000; // 5 minutes
constexpr uint32_t PULSE_MS = 1500;

static uint32_t last_pulse_ms = 0;

static void pulsePinToGnd(uint8_t pin, uint32_t pulse_ms) {
  pinMode(PIN_LED_R, OUTPUT);
  digitalWrite(PIN_LED_R, LOW);

  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(pulse_ms);
  pinMode(pin, INPUT);

  digitalWrite(PIN_LED_R, HIGH);
  pinMode(PIN_LED_R, INPUT);
}

void setup() {
  pinMode(PIN_STATUS, INPUT);
  last_pulse_ms = millis();
  // First pulse shortly after boot so bring-up is obvious.
  if (INTERVAL_MS > 15000) {
    last_pulse_ms = millis() - (INTERVAL_MS - 15000);
  }
}

void loop() {
  const uint32_t now = millis();

  if ((now - last_pulse_ms) >= INTERVAL_MS) {
    last_pulse_ms = now;
    pulsePinToGnd(PIN_STATUS, PULSE_MS);
  }
}
