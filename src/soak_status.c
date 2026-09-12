/*
 * Periodic HID status for ZMK battery soak tests.
 *
 * Types e.g.:
 *   time: 06915, power=54, mode=sleep
 *
 * With deep sleep enabled, delayed timers often do not run while asleep, so
 * status is also armed on matrix key activity (virtual finger wake).
 * last_status advances when a status attempt starts (not when it finishes),
 * so wake-key spam cannot emit multiple lines within one interval.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>

#include <zmk/battery.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <dt-bindings/zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_ZMK_SOAK_STATUS)

#define MAX_CHARS 64
#define TYPE_DELAY_MS 12
#define AFTER_KEY_DELAY_MS 400
#define TYPING_STUCK_MS 5000
/* Poll while awake; deep sleep still relies on key activity below. */
#define POLL_SEC 30

static struct k_work_delayable typing_work;
static struct k_work_delayable schedule_work;

static uint8_t chars[MAX_CHARS];
static uint8_t chars_len;
static uint8_t current_idx;
static bool key_pressed;
static bool typing_busy;
static bool self_emitting;
static int64_t last_status_ms = -1;
static int64_t typing_started_ms;

static const uint32_t digit_keycodes[10] = {
    N0, N1, N2, N3, N4, N5, N6, N7, N8, N9,
};

static void reset_typing(void) {
    current_idx = 0;
    key_pressed = false;
    chars_len = 0;
    typing_busy = false;
    typing_started_ms = 0;
    memset(chars, 0, sizeof(chars));
}

static void finish_status_ok(void) {
    reset_typing();
}

static uint32_t char_to_keycode(uint8_t ch) {
    if (ch >= '0' && ch <= '9') {
        return digit_keycodes[ch - '0'];
    }
    switch (ch) {
    case 'a':
        return A;
    case 'd':
        return D;
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
    case 'n':
        return N;
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

static bool due_for_status(void) {
    const int64_t now = k_uptime_get();

    if (last_status_ms < 0) {
        return now >= ((int64_t)CONFIG_ZMK_SOAK_STATUS_INITIAL_DELAY_SEC * 1000);
    }
    return (now - last_status_ms) >= ((int64_t)CONFIG_ZMK_SOAK_STATUS_INTERVAL_SEC * 1000);
}

static void clear_stuck_typing(void) {
    if (!typing_busy) {
        return;
    }
    if ((k_uptime_get() - typing_started_ms) < TYPING_STUCK_MS) {
        return;
    }
    LOG_WRN("soak_status: clearing stuck typing");
    k_work_cancel_delayable(&typing_work);
    reset_typing();
}

static void build_status_line(void) {
    const uint32_t uptime_s = k_uptime_get() / 1000;
    uint8_t percent = zmk_battery_state_of_charge();
    char line[MAX_CHARS];
    int n;

    if (percent > 100) {
        percent = 100;
    }

    reset_typing();

    n = snprintf(line, sizeof(line), "time: %05u, power=%02u, mode=%s\n", uptime_s, percent,
                 CONFIG_ZMK_SOAK_STATUS_MODE);
    if (n < 0) {
        return;
    }
    if (n >= (int)sizeof(line)) {
        n = (int)sizeof(line) - 1;
    }

    memcpy(chars, line, n);
    chars_len = (uint8_t)n;
    typing_busy = true;
    typing_started_ms = k_uptime_get();
    LOG_INF("soak_status: %s", line);
}

static void send_key_step(void) {
    if (current_idx >= chars_len) {
        finish_status_ok();
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
            finish_status_ok();
        }
    }
}

static void try_start_status(void) {
    clear_stuck_typing();

    if (typing_busy || !due_for_status()) {
        return;
    }

    /* Claim the interval immediately so key spam / wake edges cannot re-fire. */
    last_status_ms = k_uptime_get();
    build_status_line();
    if (!typing_busy) {
        return;
    }
    send_key_step();
}

static void typing_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    send_key_step();
}

static void schedule_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    try_start_status();
    k_work_schedule(&schedule_work, K_SECONDS(POLL_SEC));
}

#if IS_ENABLED(CONFIG_ZMK_SLEEP)
/* Deep sleep skips delayed timers; arm status after virtual-finger wakes us. */
static int soak_activity_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);

    if (ev == NULL || self_emitting || ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    clear_stuck_typing();

    if (typing_busy || !due_for_status()) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    /* Let the wake key finish, then type while still in the idle-timeout window. */
    k_work_schedule(&schedule_work, K_MSEC(AFTER_KEY_DELAY_MS));
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(soak_status_activity, soak_activity_listener);
ZMK_SUBSCRIPTION(soak_status_activity, zmk_keycode_state_changed);
#endif

static int soak_status_init(void) {
    k_work_init_delayable(&typing_work, typing_work_handler);
    k_work_init_delayable(&schedule_work, schedule_work_handler);
    reset_typing();
    last_status_ms = -1;
    k_work_schedule(&schedule_work, K_SECONDS(CONFIG_ZMK_SOAK_STATUS_INITIAL_DELAY_SEC));
    LOG_INF("soak_status: mode=%s every %d s", CONFIG_ZMK_SOAK_STATUS_MODE,
            CONFIG_ZMK_SOAK_STATUS_INTERVAL_SEC);
    return 0;
}

SYS_INIT(soak_status_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif /* CONFIG_ZMK_SOAK_STATUS */
