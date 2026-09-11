/*
 * XIAO RP2040 load generator for ZMK batt_1hz_xiao soak tests.
 *
 * Cycle (~1 Hz pulses):
 *   RIGHT, LEFT  × 4
 *   RIGHT
 *   ENTER          → ZMK soak_status types "time: …, power=…"
 *   short pause for the status line, then repeat
 *
 * Wiring (share GND + these only; no 3V3/5V/BAT):
 *   D0 → RIGHT, D1 → LEFT, D2 → ENTER
 * RP2040 = USB, nRF = LiPo.
 */

constexpr uint32_t INTERVAL_MS = 1000;
constexpr uint32_t PULSE_MS = 80;
/* Status line is ~30–40 HID keys; leave room before the next cycle. */
constexpr uint32_t AFTER_ENTER_MS = 2000;

static const uint8_t PIN_RIGHT = D0;
static const uint8_t PIN_LEFT = D1;
static const uint8_t PIN_ENTER = D2;

static void allPinsHiZ() {
  pinMode(PIN_RIGHT, INPUT);
  pinMode(PIN_LEFT, INPUT);
  pinMode(PIN_ENTER, INPUT);
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

static void tap(uint8_t pin) {
  pulsePinToGnd(pin);
  delay(INTERVAL_MS - PULSE_MS);
}

void setup() {
  allPinsHiZ();
}

void loop() {
  for (uint8_t i = 0; i < 4; i++) {
    tap(PIN_RIGHT);
    tap(PIN_LEFT);
  }
  tap(PIN_RIGHT);
  pulsePinToGnd(PIN_ENTER);
  delay(AFTER_ENTER_MS);
}
