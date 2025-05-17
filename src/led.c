#include <zephyr/drivers/led.h>
#include <zboss_api.h>


static const struct device * led_dev = DEVICE_DT_GET(DT_PARENT(DT_ALIAS(led1)));
static uint32_t led_idx = DT_NODE_CHILD_IDX(DT_ALIAS(led1));
static uint32_t on_time = 0;
static uint32_t off_time = 0;

static void do_blink_state_led(uint8_t count);

void off_state_led(uint8_t count)
{
    led_off(led_dev, led_idx);
    if (count > 1) {
        ZB_SCHEDULE_APP_ALARM(do_blink_state_led, count - 1, ZB_MILLISECONDS_TO_BEACON_INTERVAL(off_time));
    }
}

static void do_blink_state_led(uint8_t count)
{
    led_on(led_dev, led_idx);
    ZB_SCHEDULE_APP_ALARM(off_state_led, count, ZB_MILLISECONDS_TO_BEACON_INTERVAL(on_time));
}

void blink_state_led(uint32_t on_ms, uint32_t off_ms, uint8_t count)
{
    ZB_SCHEDULE_APP_ALARM_CANCEL(do_blink_state_led, ZB_ALARM_ANY_PARAM);
    on_time = on_ms;
    off_time = off_ms;
    do_blink_state_led(count);
}

void set_state_led(bool state)
{
    led_set_brightness(led_dev, led_idx, state);
}
