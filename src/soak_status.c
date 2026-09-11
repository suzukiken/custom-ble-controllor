/*
 * HID status lines for ZMK battery soak tests.
 *
 * Default (batt_1hz): after host Enter from the RP2040 cycle, type:
 *   time: 06915, power=54
 * Optional when voltage is available:
 *   time: 06915, power=54, mv=3921
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>

#include <zmk/battery.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <dt-bindings/zmk/hid_usage.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
#include <dt-bindings/zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_ZMK_SOAK_STATUS)

#define MAX_CHARS 64
#define TYPE_DELAY_MS 12
#define AFTER_ENTER_DELAY_MS 120

static struct k_work_delayable typing_work;
static struct k_work_delayable schedule_work;

static uint8_t chars[MAX_CHARS];
static uint8_t chars_len;
static uint8_t current_idx;
static bool key_pressed;
static bool typing_busy;
static bool self_emitting;

static const uint32_t digit_keycodes[10] = {
    N0, N1, N2, N3, N4, N5, N6, N7, N8, N9,
};

static void reset_typing(void) {
    current_idx = 0;
    key_pressed = false;
    chars_len = 0;
    typing_busy = false;
    memset(chars, 0, sizeof(chars));
}

static uint32_t char_to_keycode(uint8_t ch) {
    if (ch >= '0' && ch <= '9') {
        return digit_keycodes[ch - '0'];
    }
    switch (ch) {
    case 'a':
        return A;
    case 'e':
        return E;
    case 'i':
        return I;
    case 'm':
        return M;
    case 'o':
        return O;
    case 'p':
        return P;
    case 'r':
        return R;
    case 't':
        return T;
    case 'v':
        return V;
    case 'w':
        return W;
    case ' ':
        return SPACE;
    case ',':
        return COMMA;
    case '=':
        return EQUAL;
    case ':':
        /* US QWERTY: Shift + ; */
        return COLON;
    case '\n':
        return ENTER;
    default:
        return 0;
    }
}

static uint16_t read_voltage_mv(void) {
#if DT_HAS_CHOSEN(zmk_battery)
    const struct device *batt = DEVICE_DT_GET(DT_CHOSEN(zmk_battery));
    struct sensor_value val;
    int rc;

    if (!device_is_ready(batt)) {
        return 0;
    }
    rc = sensor_sample_fetch_chan(batt, SENSOR_CHAN_VOLTAGE);
    if (rc != 0) {
        return 0;
    }
    rc = sensor_channel_get(batt, SENSOR_CHAN_VOLTAGE, &val);
    if (rc != 0) {
        return 0;
    }
    if (val.val1 < 0) {
        return 0;
    }
    return (uint16_t)(val.val1 * 1000 + val.val2 / 1000);
#else
    return 0;
#endif
}

static void build_status_line(void) {
    const uint32_t uptime_s = k_uptime_get() / 1000;
    uint8_t percent = zmk_battery_state_of_charge();
    uint16_t mv = read_voltage_mv();
    char line[MAX_CHARS];
    int n;

    if (percent > 100) {
        percent = 100;
    }

    reset_typing();

    /* RP2040 already sent Enter; do not prepend another newline. */
    if (mv > 0) {
        n = snprintf(line, sizeof(line), "time: %05u, power=%02u, mv=%u\n", uptime_s, percent,
                     mv);
    } else {
        n = snprintf(line, sizeof(line), "time: %05u, power=%02u\n", uptime_s, percent);
    }
    if (n < 0) {
        return;
    }
    if (n >= (int)sizeof(line)) {
        n = (int)sizeof(line) - 1;
    }

    memcpy(chars, line, n);
    chars_len = (uint8_t)n;
    typing_busy = true;
    LOG_INF("soak_status: %s", line);
}

static void send_key_step(void) {
    if (current_idx >= chars_len) {
        reset_typing();
        return;
    }

    uint32_t keycode = char_to_keycode(chars[current_idx]);
    if (!keycode) {
        LOG_WRN("soak_status: bad char '%c' (%u)", chars[current_idx], chars[current_idx]);
        reset_typing();
        return;
    }

    bool press = !key_pressed;
    self_emitting = true;
    raise_zmk_keycode_state_changed_from_encoded(keycode, press, k_uptime_get());
    self_emitting = false;
    key_pressed = press;

    if (press) {
        k_work_schedule(&typing_work, K_MSEC(TYPE_DELAY_MS));
    } else {
        current_idx++;
        if (current_idx < chars_len) {
            k_work_schedule(&typing_work, K_MSEC(TYPE_DELAY_MS));
        } else {
            reset_typing();
        }
    }
}

static void typing_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    send_key_step();
}

static void schedule_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (!typing_busy) {
        build_status_line();
        send_key_step();
    } else {
        LOG_DBG("soak_status: skip, still typing");
    }

#if !IS_ENABLED(CONFIG_ZMK_SOAK_STATUS_ON_ENTER)
    k_work_schedule(&schedule_work, K_SECONDS(CONFIG_ZMK_SOAK_STATUS_INTERVAL_SEC));
#endif
}

#if IS_ENABLED(CONFIG_ZMK_SOAK_STATUS_ON_ENTER)
static int soak_enter_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);

    if (ev == NULL || self_emitting || typing_busy || !ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    if (ev->usage_page != HID_USAGE_KEY ||
        ev->keycode != HID_USAGE_KEY_KEYBOARD_RETURN) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    /* After RP2040's Enter settles, type the status on the new line. */
    k_work_schedule(&schedule_work, K_MSEC(AFTER_ENTER_DELAY_MS));
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(soak_status_enter, soak_enter_listener);
ZMK_SUBSCRIPTION(soak_status_enter, zmk_keycode_state_changed);
#endif

static int soak_status_init(void) {
    k_work_init_delayable(&typing_work, typing_work_handler);
    k_work_init_delayable(&schedule_work, schedule_work_handler);
    reset_typing();
#if IS_ENABLED(CONFIG_ZMK_SOAK_STATUS_ON_ENTER)
    LOG_INF("soak_status: after Enter");
#else
    k_work_schedule(&schedule_work, K_SECONDS(CONFIG_ZMK_SOAK_STATUS_INITIAL_DELAY_SEC));
    LOG_INF("soak_status: every %d s", CONFIG_ZMK_SOAK_STATUS_INTERVAL_SEC);
#endif
    return 0;
}

SYS_INIT(soak_status_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif /* CONFIG_ZMK_SOAK_STATUS */
