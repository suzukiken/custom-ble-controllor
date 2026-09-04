/*
 * XIAO RP2040 key-press tester for stacked XIAO nRF52840 (ZMK).
 *
 * Every INTERVAL_MS, pulse TARGET_PIN low (to GND) for PULSE_MS,
 * then return to Hi-Z (INPUT). Other D0-D10 pins stay Hi-Z.
 *
 * Power: give this board its own USB power. Share GND + GPIO only
 * with the battery-powered nRF52840. Do not tie 3V3/5V/BAT together.
 */

constexpr uint8_t PIN_COUNT = 11; // D0 .. D10
constexpr uint8_t TARGET_PIN = D0;
constexpr uint32_t INTERVAL_MS = 30000;
constexpr uint32_t PULSE_MS = 50;

static const uint8_t kPins[PIN_COUNT] = {
    D0, D1, D2, D3, D4, D5, D6, D7, D8, D9, D10};

static void allPinsHiZ() {
  for (uint8_t i = 0; i < PIN_COUNT; i++) {
    pinMode(kPins[i], INPUT);
  }
}

static void pulsePinToGnd(uint8_t pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(PULSE_MS);
  pinMode(pin, INPUT);
}

void setup() {
  allPinsHiZ();
}

void loop() {
  pulsePinToGnd(TARGET_PIN);
  delay(INTERVAL_MS - PULSE_MS);
}
