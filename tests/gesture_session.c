#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>

#define ARG_UNUSED(x) (void)(x)
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define K_FOREVER -1
#define K_NO_WAIT 0
#define K_MSEC(ms) (ms)
#define LOG_MODULE_DECLARE(...)
#define LOG_ERR(...) logged_errors++
#define LOG_DBG(...)
#define DT_HAS_COMPAT_STATUS_OKAY(...) 1
#define DT_INST_PROP(n, prop) 12
#define DT_INST_FOREACH_STATUS_OKAY(...)
#define ZMK_BEHAVIOR_OPAQUE 0
#define K_MUTEX_DEFINE(name) int name
#define k_mutex_lock(lock, timeout) ((void)(lock), (void)(timeout))
#define k_mutex_unlock(lock) ((void)(lock))
#define K_WORK_DELAYABLE_DEFINE(name, fn) struct k_work_delayable name = {.handler = fn}

enum { LALT = 1, LSHIFT, TAB, LGUI, LCTRL, W, GRAVE };
enum { INPUT_EV_KEY = 1, INPUT_EV_REL = 2 };
enum { INPUT_BTN_TOUCH = 100, INPUT_BTN_NORTH, INPUT_BTN_EAST, INPUT_BTN_SOUTH,
       INPUT_BTN_WEST };
enum { ZMK_INPUT_PROC_CONTINUE, ZMK_INPUT_PROC_STOP };

struct device { const void *config; void *data; };
struct input_event { uint16_t type, code; int32_t value; };
struct zmk_input_processor_state { int unused; };
struct zmk_input_processor_driver_api {
    int (*handle_event)(const struct device *, struct input_event *, uint32_t, uint32_t,
                        struct zmk_input_processor_state *);
};
struct k_work { int unused; };
struct k_work_delayable {
    void (*handler)(struct k_work *);
    int64_t due;
    bool pending;
};
struct zmk_behavior_binding { uint32_t param1; };
struct zmk_behavior_binding_event { int64_t timestamp; };
struct behavior_driver_api {
    int (*binding_pressed)(struct zmk_behavior_binding *, struct zmk_behavior_binding_event);
    int (*binding_released)(struct zmk_behavior_binding *, struct zmk_behavior_binding_event);
};
struct key_event { uint32_t key; bool pressed; };
static struct key_event events[1024];
static size_t event_count;
static int64_t now;
static unsigned logged_errors;
static bool host_keys[8];

static int64_t k_uptime_get(void) { return now; }
static int k_work_reschedule(struct k_work_delayable *work, int64_t delay) {
    work->pending = true;
    work->due = now + delay;
    return 0;
}
static int k_work_cancel_delayable(struct k_work_delayable *work) {
    work->pending = false;
    return 0;
}
static int raise_zmk_keycode_state_changed_from_encoded(uint32_t key, bool pressed,
                                                       int64_t timestamp) {
    (void)timestamp;
    assert(event_count < sizeof(events) / sizeof(events[0]));
    assert(key < sizeof(host_keys) / sizeof(host_keys[0]));
    assert(host_keys[key] != pressed);
    host_keys[key] = pressed;
    events[event_count++] = (struct key_event){key, pressed};
    return 0;
}

#include "../src/behavior_win_switcher.c"
#undef DT_DRV_COMPAT
#include "../src/input_processor_swipe_arbiter.c"

static void drain(void) {
    unsigned limit = 1000;
    while (step_work.pending) {
        assert(limit--);
        now = step_work.due;
        step_work.pending = false;
        step_work.handler(NULL);
    }
}

static int press(uint32_t action) {
    struct zmk_behavior_binding binding = {.param1 = action};
    return on_keymap_binding_pressed(&binding, (struct zmk_behavior_binding_event){0});
}

static void lift(void) {
    struct zmk_behavior_binding binding = {.param1 = TOUCAN_GESTURE_CONTACT};
    on_keymap_binding_released(&binding, (struct zmk_behavior_binding_event){0});
}

static void reset(void) {
    lift();
    drain();
    for (size_t i = 1; i < sizeof(host_keys) / sizeof(host_keys[0]); i++) {
        assert(!host_keys[i]);
    }
    event_count = 0;
    logged_errors = 0;
}

static void expect(const struct key_event *expected, size_t count) {
    assert(event_count == count);
    for (size_t i = 0; i < count; i++) {
        assert(events[i].key == expected[i].key);
        assert(events[i].pressed == expected[i].pressed);
    }
}

#define EXPECT(...) do { \
    const struct key_event expected[] = {__VA_ARGS__}; \
    expect(expected, sizeof(expected) / sizeof(expected[0])); \
} while (0)

static void test_contact_and_right(void) {
    reset();
    press(TOUCAN_GESTURE_RIGHT); // No inferred contact from a swipe.
    drain();
    assert(event_count == 0);
    press(TOUCAN_GESTURE_CONTACT);
    drain();
    now += 10000;
    drain();
    EXPECT({LALT, true});
    press(TOUCAN_GESTURE_CONTACT); // Duplicate contact does not press Alt twice.
    press(TOUCAN_GESTURE_RIGHT);
    drain();
    assert(host_keys[LALT]);
    press(TOUCAN_GESTURE_LEFT);
    drain();
    lift();
    drain();
    EXPECT({LALT, true}, {TAB, true}, {TAB, false},
           {LSHIFT, true}, {TAB, true}, {TAB, false}, {LSHIFT, false}, {LALT, false});
}

static void test_left(void) {
    reset();
    press(TOUCAN_GESTURE_CONTACT);
    press(TOUCAN_GESTURE_LEFT);
    lift(); // Lift before the queued sequence runs must not drop the action.
    drain();
    EXPECT({LALT, true}, {LSHIFT, true}, {TAB, true}, {TAB, false},
           {LSHIFT, false}, {LALT, false});
}

static void test_up(void) {
    reset();
    press(TOUCAN_GESTURE_CONTACT);
    press(TOUCAN_GESTURE_UP);
    press(TOUCAN_GESTURE_RIGHT); // Task View ends navigation until lift.
    lift();
    drain();
    EXPECT({LALT, true}, {LALT, false}, {LGUI, true},
           {TAB, true}, {TAB, false}, {LGUI, false});
}

static void test_down(void) {
    reset();
    press(TOUCAN_GESTURE_CONTACT);
    press(TOUCAN_GESTURE_DOWN);
    lift();
    drain();
    EXPECT({LALT, true}, {LALT, false}, {LCTRL, true},
           {W, true}, {W, false}, {LCTRL, false});
}

static void test_rapid_contacts_and_overflow(void) {
    reset();
    press(TOUCAN_GESTURE_CONTACT);
    lift();
    press(TOUCAN_GESTURE_CONTACT);
    press(TOUCAN_GESTURE_RIGHT);
    lift();
    drain();
    EXPECT({LALT, true}, {LALT, false}, {LALT, true},
           {TAB, true}, {TAB, false}, {LALT, false});
    reset();
    assert(press(99) == -EINVAL);
    press(TOUCAN_GESTURE_CONTACT);
    drain(); // Alt is actually down when the queue later overflows.
    for (int i = 0; i < MAX_STEPS; i++) {
        lift();
        press(TOUCAN_GESTURE_CONTACT);
    }
    lift();
    drain();
    assert(logged_errors > 1);
    assert(!host_keys[LALT]);
}

static void test_arbiter_contact_transition(void) {
    const struct arbiter_config config = {.lead = 2, .max_samples = 8, .reset_ms = 400};
    struct arbiter_data data = {0};
    const struct device dev = {.config = &config, .data = &data};
    struct input_event direction = {.type = INPUT_EV_KEY, .code = INPUT_BTN_EAST, .value = 1};
    assert(arbiter_handle_event(&dev, &direction, 0, 0, NULL) == ZMK_INPUT_PROC_STOP);
    assert(arbiter_handle_event(&dev, &direction, 0, 0, NULL) == ZMK_INPUT_PROC_CONTINUE);
    assert(data.emitted);
    struct input_event contact = {.type = INPUT_EV_KEY,
                                  .code = TOUCAN_INPUT_BTN_THREE_FINGER, .value = 0};
    arbiter_handle_event(&dev, &contact, 0, 0, NULL);
    assert(!data.emitted); // 3 -> 2 fingers resets even without a TOUCH-up event.
    contact.value = 1;
    arbiter_handle_event(&dev, &contact, 0, 0, NULL);
    assert(arbiter_handle_event(&dev, &direction, 0, 0, NULL) == ZMK_INPUT_PROC_STOP);
    assert(arbiter_handle_event(&dev, &direction, 0, 0, NULL) == ZMK_INPUT_PROC_CONTINUE);
}

static int raw_swipe(const struct device *dev, uint16_t code) {
    struct input_event sample = {.type = INPUT_EV_KEY, .code = code, .value = 1};
    now += 40;
    const int result = arbiter_handle_event(dev, &sample, 0, 0, NULL);
    if (result == ZMK_INPUT_PROC_CONTINUE) {
        press(code == INPUT_BTN_WEST ? TOUCAN_GESTURE_RIGHT : TOUCAN_GESTURE_LEFT);
    }
    sample.value = 0;
    arbiter_handle_event(dev, &sample, 0, 0, NULL);
    drain();
    return result;
}

static void test_continuous_swipes(void) {
    reset();
    const struct arbiter_config config = {.lead = 2, .max_samples = 8, .reset_ms = 400};
    struct arbiter_data data = {0};
    const struct device dev = {.config = &config, .data = &data};
    press(TOUCAN_GESTURE_CONTACT);
    drain();
    assert(raw_swipe(&dev, INPUT_BTN_WEST) == ZMK_INPUT_PROC_STOP);
    assert(raw_swipe(&dev, INPUT_BTN_WEST) == ZMK_INPUT_PROC_CONTINUE); // Right.
    assert(raw_swipe(&dev, INPUT_BTN_WEST) == ZMK_INPUT_PROC_STOP); // No raw-repeat flood.
    assert(raw_swipe(&dev, INPUT_BTN_EAST) == ZMK_INPUT_PROC_STOP);
    assert(raw_swipe(&dev, INPUT_BTN_WEST) == ZMK_INPUT_PROC_STOP); // One reversal sample is noise.
    assert(raw_swipe(&dev, INPUT_BTN_EAST) == ZMK_INPUT_PROC_STOP);
    assert(raw_swipe(&dev, INPUT_BTN_EAST) == ZMK_INPUT_PROC_CONTINUE); // Left, without lifting.
    now += 401;
    assert(raw_swipe(&dev, INPUT_BTN_EAST) == ZMK_INPUT_PROC_STOP);
    assert(raw_swipe(&dev, INPUT_BTN_EAST) == ZMK_INPUT_PROC_CONTINUE); // Another left after a pause.
    assert(raw_swipe(&dev, INPUT_BTN_WEST) == ZMK_INPUT_PROC_STOP);
    assert(raw_swipe(&dev, INPUT_BTN_WEST) == ZMK_INPUT_PROC_CONTINUE); // Right again.
    assert(host_keys[LALT]);
    lift();
    drain();
    EXPECT({LALT, true}, {TAB, true}, {TAB, false},
           {LSHIFT, true}, {TAB, true}, {TAB, false}, {LSHIFT, false},
           {LSHIFT, true}, {TAB, true}, {TAB, false}, {LSHIFT, false},
           {TAB, true}, {TAB, false}, {LALT, false});
}

int main(void) {
    test_contact_and_right();
    test_left();
    test_up();
    test_down();
    test_rapid_contacts_and_overflow();
    test_arbiter_contact_transition();
    test_continuous_swipes();
    reset();
    puts("PASS: Windows contact lifetime, directions, quick lift, raw-repeat suppression, "
         "rapid contacts, overflow cleanup, three-to-two-finger rearm, continuous reversals, "
         "same-direction pause rearm");
    return 0;
}
