/*
 * Periodic HID status for ZMK battery soak tests (sleep_xiao / awake_xiao).
 *
 * Every INTERVAL seconds, types e.g.:
 *   time: 06915, power=54, mode=sleep
 * followed by Enter. mode=awake when deep sleep is disabled.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>

#include <zmk/battery.h>
#include <zmk/events/keycode_state_changed.h>
#include <dt-bindings/zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_ZMK_SOAK_STATUS)

#define MAX_CHARS 56
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
    case 'a':
        return A;
    case 'e':
        return E;
    case 'i':
        return I;
    case 'k':
        return K;
    case 'l':
        return L;
    case 'm':
        return M;
    case 'o':
        return O;
    case 'p':
        return P;
    case 'r':
        return R;
    case 's':
        return S;
    case 't':
        return T;
    case 'w':
        return W;
    case ' ':
        return SPACE;
    case ',':
        return COMMA;
    case '=':
        return EQUAL;
    case ':':
        return COLON;
    case '\n':
        return ENTER;
    default:
        return 0;
    }
}

static void build_status_line(void) {
    const uint32_t uptime_s = k_uptime_get() / 1000;
    uint8_t percent = zmk_battery_state_of_charge();
    const char *mode = IS_ENABLED(CONFIG_ZMK_SLEEP) ? "sleep" : "awake";
    char line[MAX_CHARS];
    int n;

    if (percent > 100) {
        percent = 100;
    }

    reset_typing();

    n = snprintf(line, sizeof(line), "time: %05u, power=%02u, mode=%s\n", uptime_s, percent,
                 mode);
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
