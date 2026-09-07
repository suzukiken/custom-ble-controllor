/*
 * XIAO RP2040 digit-line tester for stacked XIAO nRF52840 (ZMK batt_1hz_xiao).
 *
 * Cycles D0→D1→…→D9→D10, pulsing each pin LOW (to GND) for PULSE_MS,
 * once per INTERVAL_MS. Other pins stay Hi-Z (INPUT).
 *
 * Expected HID on the nRF side: 0 1 2 3 4 5 6 7 8 9 Enter
 * → one Notes line per 11 pulses ≈ 11 * INTERVAL_MS of runtime.
 *
 * Power: USB on this board. nRF on LiPo. Share GND + D0–D10 only.
 * Do not tie 3V3 / 5V / BAT together.
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
  digitalWrite(PIN_LED_R, LOW); // active low

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
