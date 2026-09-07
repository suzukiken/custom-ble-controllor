/*
 * XIAO nRF52840 power button → System OFF
 *
 * Wiring: D0 ----[ switch ]---- GND  (internal pull-up)
 *
 * ON:  wake from System OFF (GPIO sense on D0 low), then hold 1s to stay on.
 *      If released before 1s, go back to System OFF (ignores accidental taps).
 * OFF: while running, hold 3s, release, then enter System OFF.
 *
 * USB VBUS present: skip the 1s ON confirm (easy to flash / debug).
 *
 * LEDs (active-low, feedback only):
 *   blue blink  — confirming 1s ON hold
 *   red blink   — confirming 3s OFF hold
 *   green flash — ON accepted
 * Idle ON: LEDs off.
 *
 * Board: Seeed nRF52 Boards → Seeed XIAO nRF52840
 */

#include <Adafruit_TinyUSB.h>
#include <nrf.h>
#include <nrf_gpio.h>

constexpr uint8_t BTN_PIN = D0;
constexpr uint32_t ON_HOLD_MS = 1000;
constexpr uint32_t OFF_HOLD_MS = 3000;

static bool buttonPressed() { return digitalRead(BTN_PIN) == LOW; }

static bool vbusPresent() {
  return (NRF_POWER->USBREGSTATUS & POWER_USBREGSTATUS_VBUSDETECT_Msk) != 0;
}

static void ledsOff() {
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
}

static void enterSystemOff() {
  ledsOff();

  // Must release first — otherwise sense sees LOW and wakes immediately.
  while (buttonPressed()) {
    delay(10);
  }
  delay(50);

  // g_ADigitalPinMap: Arduino D# → NRF GPIO number (Seeed/Adafruit core).
  const uint32_t nrf_pin = g_ADigitalPinMap[BTN_PIN];
  nrf_gpio_cfg_sense_input(nrf_pin, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);

  // Deepest sleep; wake = full reset then setup().
  NRF_POWER->SYSTEMOFF = 1;
  while (true) {
    // never returns
  }
}

// Returns true if ON is confirmed (held ON_HOLD_MS, or USB debugging).
static bool confirmPowerOn() {
  if (vbusPresent()) {
    return true;
  }

  if (!buttonPressed()) {
    return false;
  }

  const uint32_t start = millis();
  while (buttonPressed()) {
    const uint32_t held = millis() - start;
    if (held >= ON_HOLD_MS) {
      digitalWrite(LED_GREEN, LOW);
      delay(150);
      digitalWrite(LED_GREEN, HIGH);
      // Wait for release so the same press does not start OFF timing.
      while (buttonPressed()) {
        delay(10);
      }
      delay(50);
      return true;
    }
    // Blue blink while confirming ON
    digitalWrite(LED_BLUE, (held / 100) % 2 ? HIGH : LOW);
    delay(10);
  }
  digitalWrite(LED_BLUE, HIGH);
  return false;
}

void setup() {
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  ledsOff();

  pinMode(BTN_PIN, INPUT_PULLUP);

  if (!confirmPowerOn()) {
    enterSystemOff();
  }
}

void loop() {
  static uint32_t press_start = 0;
  static bool armed = false;

  if (buttonPressed()) {
    if (!armed) {
      press_start = millis();
      armed = true;
    }
    const uint32_t held = millis() - press_start;
    if (held >= OFF_HOLD_MS) {
      // Red solid: OFF accepted — wait for release then System OFF
      digitalWrite(LED_RED, LOW);
      enterSystemOff();
    } else if (held >= 500) {
      // Red blink: approaching OFF
      digitalWrite(LED_RED, (held / 150) % 2 ? HIGH : LOW);
    }
  } else {
    armed = false;
    press_start = 0;
    digitalWrite(LED_RED, HIGH);
  }

  delay(10);
}
