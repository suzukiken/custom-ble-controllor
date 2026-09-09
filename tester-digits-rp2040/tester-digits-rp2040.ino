/*
 * XIAO RP2040 digit-line load generator for ZMK batt_1hz_xiao soak tests.
 *
 * Cycles D0→…→D10 (~1 Hz): digits 0–9 + Enter on the nRF (ZMK).
 * Status lines (uptime / % / mV) are typed by the ZMK soak-status module
 * every few minutes — this sketch only creates continuous HID load.
 *
 * Power: USB here, LiPo on nRF. Share GND + D0–D10 only (no 3V3/5V/BAT).
 */

constexpr uint8_t PIN_COUNT = 11; // D0 .. D10
constexpr uint32_t INTERVAL_MS = 1000;
constexpr uint32_t PULSE_MS = 80;

static const uint8_t kPins[PIN_COUNT] = {
    D0, D1, D2, D3, D4, D5, D6, D7, D8, D9, D10};

static void allPinsHiZ() {
  for (uint8_t i = 0; i < PIN_COUNT; i++) {
    pinMode(kPins[i], INPUT);
  }
}

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
  allPinsHiZ();
}

void loop() {
  for (uint8_t i = 0; i < PIN_COUNT; i++) {
    pulsePinToGnd(kPins[i]);
    delay(INTERVAL_MS - PULSE_MS);
  }
}
