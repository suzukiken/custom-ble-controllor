/*
 * Intentional high power draw for draining a small LiPo (e.g. to ~50% SoC)
 * before measuring charge current.
 *
 * - RGB LEDs on solid (XIAO nRF52840)
 * - Cooperative busy thread (discourage CPU idle)
 * - Frequent HID presses while connected (radio activity)
 * - Periodic status: time: …, power=…, mode=drain
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>

#include <zmk/battery.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <dt-bindings/zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_ZMK_POWER_DRAIN)

#define MAX_CHARS 48
#define TYPE_DELAY_MS 8
#define TYPING_STUCK_MS 5000

static struct k_work_delayable typing_work;
static struct k_work_delayable spam_work;
static struct k_work_delayable status_work;

static uint8_t chars[MAX_CHARS];
static uint8_t chars_len;
static uint8_t current_idx;
static bool key_pressed;
static bool typing_busy;
static bool self_emitting;
static bool spam_down;
static int64_t typing_started_ms;

static const uint32_t digit_keycodes[10] = {
    N0, N1, N2, N3, N4, N5, N6, N7, N8, N9,
};

#if DT_NODE_EXISTS(DT_ALIAS(led0))
static const struct gpio_dt_spec led_r = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
#endif
#if DT_NODE_EXISTS(DT_ALIAS(led1))
static const struct gpio_dt_spec led_g = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
#endif
#if DT_NODE_EXISTS(DT_ALIAS(led2))
static const struct gpio_dt_spec led_b = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
#endif

static void leds_on(void) {
#if DT_NODE_EXISTS(DT_ALIAS(led0))
    if (gpio_is_ready_dt(&led_r)) {
        gpio_pin_configure_dt(&led_r, GPIO_OUTPUT_ACTIVE);
    }
#endif
#if DT_NODE_EXISTS(DT_ALIAS(led1))
    if (gpio_is_ready_dt(&led_g)) {
        gpio_pin_configure_dt(&led_g, GPIO_OUTPUT_ACTIVE);
    }
#endif
#if DT_NODE_EXISTS(DT_ALIAS(led2))
    if (gpio_is_ready_dt(&led_b)) {
        gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_ACTIVE);
    }
#endif
}

static void reset_typing(void) {
    current_idx = 0;
    key_pressed = false;
    chars_len = 0;
    typing_busy = false;
    typing_started_ms = 0;
    memset(chars, 0, sizeof(chars));
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

static void clear_stuck_typing(void) {
    if (!typing_busy) {
        return;
    }
    if ((k_uptime_get() - typing_started_ms) < TYPING_STUCK_MS) {
        return;
    }
    LOG_WRN("power_drain: clearing stuck typing");
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
    n = snprintf(line, sizeof(line), "time: %05u, power=%02u, mode=drain\n", uptime_s, percent);
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
    LOG_INF("power_drain: %s", line);
}

static void send_key_step(void) {
    if (current_idx >= chars_len) {
        reset_typing();
        return;
    }

    uint32_t keycode = char_to_keycode(chars[current_idx]);
    if (!keycode) {
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

static void spam_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    clear_stuck_typing();
    if (!typing_busy) {
        self_emitting = true;
        raise_zmk_keycode_state_changed_from_encoded(SPACE, !spam_down, k_uptime_get());
        self_emitting = false;
        spam_down = !spam_down;
    }

    k_work_schedule(&spam_work, K_MSEC(CONFIG_ZMK_POWER_DRAIN_HID_MS));
}

static void status_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    clear_stuck_typing();
    if (!typing_busy) {
        build_status_line();
        if (typing_busy) {
            send_key_step();
        }
    }

    k_work_schedule(&status_work, K_MSEC(CONFIG_ZMK_POWER_DRAIN_STATUS_MS));
}

/* Never sleep: burn CPU while other work runs. */
static void drain_cpu_thread(void *a, void *b, void *c) {
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    while (1) {
        k_busy_wait(2000);
    }
}

K_THREAD_DEFINE(drain_cpu_tid, 512, drain_cpu_thread, NULL, NULL, NULL, 14, 0, 0);

static int power_drain_init(void) {
    leds_on();
    k_work_init_delayable(&typing_work, typing_work_handler);
    k_work_init_delayable(&spam_work, spam_work_handler);
    k_work_init_delayable(&status_work, status_work_handler);
    reset_typing();

    k_work_schedule(&spam_work, K_MSEC(500));
    k_work_schedule(&status_work, K_MSEC(2000));

    LOG_INF("power_drain: LEDs+CPU+HID spam=%dms status=%dms", CONFIG_ZMK_POWER_DRAIN_HID_MS,
            CONFIG_ZMK_POWER_DRAIN_STATUS_MS);
    return 0;
}

SYS_INIT(power_drain_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif /* CONFIG_ZMK_POWER_DRAIN */
