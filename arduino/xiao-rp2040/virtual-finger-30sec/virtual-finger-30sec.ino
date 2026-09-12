/*
 * XIAO RP2040 virtual-finger-30sec for ZMK sleep_xiao / awake_xiao.
 *
 * Every INTERVAL_MS, pulse TARGET_PIN low (to GND) for PULSE_MS,
 * then return to Hi-Z (INPUT). Other D0-D10 pins stay Hi-Z.
 *
 * PULSE_MS must cover nRF wake-from-sleep + ZMK debounce.
 *
 * Pair: arduino/.../virtual-finger-30sec  +  ZMK sleep_xiao or awake_xiao
 * Power: USB on this board. Share GND + GPIO only with LiPo nRF.
 */

constexpr uint8_t PIN_COUNT = 11; // D0 .. D10
constexpr uint8_t TARGET_PIN = D0;
constexpr uint32_t INTERVAL_MS = 30000;
// Wake from ZMK deep sleep can take tens–hundreds of ms; keep LOW long enough.
constexpr uint32_t PULSE_MS = 500;

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
  pulsePinToGnd(TARGET_PIN);
  delay(INTERVAL_MS - PULSE_MS);
}
