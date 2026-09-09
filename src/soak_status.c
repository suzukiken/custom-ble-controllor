/*
 * Periodic HID status for ZMK battery soak tests.
 *
 * Types: u<uptime_s>p<percent>v<mv><Enter>
 * Example: u3600p87v3921
 *
 * Interleaves with RP2040 digit spam; Notes lines matching ^u[0-9]+p are status.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include <zmk/battery.h>
#include <zmk/events/keycode_state_changed.h>
#include <dt-bindings/zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_ZMK_SOAK_STATUS)

#define MAX_CHARS 48
#define TYPE_DELAY_MS 12

static struct k_work_delayable typing_work;
static struct k_work_delayable schedule_work;

static uint8_t chars[MAX_CHARS];
static uint8_t chars_len;
static uint8_t current_idx;
static bool key_pressed;
static bool typing_busy;

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
    case 'u':
        return U;
    case 'p':
        return P;
    case 'v':
        return V;
    case '\n':
        return ENTER;
    default:
        return 0;
    }
}

static void append_uint(uint32_t v) {
    uint8_t tmp[10];
    uint8_t n = 0;
    if (v == 0) {
        if (chars_len < MAX_CHARS) {
            chars[chars_len++] = '0';
        }
        return;
    }
    while (v > 0 && n < sizeof(tmp)) {
        tmp[n++] = '0' + (v % 10);
        v /= 10;
    }
    while (n > 0 && chars_len < MAX_CHARS) {
        chars[chars_len++] = tmp[--n];
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
    /* val1 = volts, val2 = µV fraction in Zephyr sensor API */
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

    if (percent > 100) {
        percent = 100;
    }

    reset_typing();
    /* End any in-progress digit line, then status, then newline. */
    chars[chars_len++] = '\n';
    chars[chars_len++] = 'u';
    append_uint(uptime_s);
    chars[chars_len++] = 'p';
    append_uint(percent);
    chars[chars_len++] = 'v';
    append_uint(mv);
    chars[chars_len++] = '\n';

    typing_busy = true;
    LOG_INF("soak_status: u=%u p=%u v=%u", uptime_s, percent, mv);
}

static void send_key_step(void) {
    if (current_idx >= chars_len) {
        reset_typing();
        return;
    }

    uint32_t keycode = char_to_keycode(chars[current_idx]);
    if (!keycode) {
        LOG_WRN("soak_status: bad char %u", chars[current_idx]);
        reset_typing();
        return;
    }

    bool press = !key_pressed;
    raise_zmk_keycode_state_changed_from_encoded(keycode, press, k_uptime_get());
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

    k_work_schedule(&schedule_work, K_SECONDS(CONFIG_ZMK_SOAK_STATUS_INTERVAL_SEC));
}

static int soak_status_init(void) {
    k_work_init_delayable(&typing_work, typing_work_handler);
    k_work_init_delayable(&schedule_work, schedule_work_handler);
    reset_typing();
    k_work_schedule(&schedule_work, K_SECONDS(CONFIG_ZMK_SOAK_STATUS_INITIAL_DELAY_SEC));
    LOG_INF("soak_status: every %d s", CONFIG_ZMK_SOAK_STATUS_INTERVAL_SEC);
    return 0;
}

SYS_INIT(soak_status_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif /* CONFIG_ZMK_SOAK_STATUS */
