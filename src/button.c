#include <zephyr/logging/log.h>
#include <zephyr/input/input.h>
#include <zboss_api.h>
#include "zigbee.h"
#include "scene_codes.h"


#define LONG_PRESS_INTERVAL 1000

LOG_MODULE_REGISTER(button, LOG_LEVEL_INF);

static int pressed_buttons = 0;

static void continous_press_timer(zb_uint8_t scene_id)
{
    if (__builtin_popcount(pressed_buttons) > 1) {
        return;
    }
    ZB_SCHEDULE_APP_ALARM(continous_press_timer, scene_id, ZB_MILLISECONDS_TO_BEACON_INTERVAL(LONG_PRESS_INTERVAL));
    send_scene(scene_id);
}

static void button_handler(struct input_event *evt, void *user_data)
{
    LOG_INF("Button event. type=%d, code=0x%x, value=%d", evt->type, evt->code, evt->value);

    if (evt->type != INPUT_EV_KEY) {
        return;
    }
    uint16_t scene_type = evt->code >> SCENE_TYPE_SHIFT;
    uint8_t button = evt->code & BUTTON_MASK;
    bool block = false;
    // do not trigger on multiple keys
    if (scene_type == SCENE_TYPE_KEY && evt->value == BUTTON_PRESSED) {
        pressed_buttons |= (1 << (button - 1));
    }
    if (__builtin_popcount(pressed_buttons) > 1) {
        block = true;
    }
    if (scene_type == SCENE_TYPE_KEY && evt->value == BUTTON_RELEASED) {
        pressed_buttons &= ~(1 << (button - 1));
    }
    if (block) {
        return;
    }

    uint16_t scene_id = SCENE_NONE;
    if (
        (scene_type == SCENE_TYPE_LONG && evt->value == BUTTON_PRESSED) || // long press activation
        (scene_type == SCENE_TYPE_SINGLE && evt->value == BUTTON_RELEASED) || // single press
        (scene_type == SCENE_TYPE_DOUBLE && evt->value == BUTTON_RELEASED) // double press
    ) {
        scene_id = evt->code;
        if (scene_type == SCENE_TYPE_LONG && evt->value == BUTTON_PRESSED) {
            ZB_SCHEDULE_APP_ALARM(
                continous_press_timer,
                SCENE_CODE(SCENE_TYPE_REPEAT, scene_id & BUTTON_MASK),
                ZB_MILLISECONDS_TO_BEACON_INTERVAL(LONG_PRESS_INTERVAL)
            );
        }
    }
    else if (scene_type == SCENE_TYPE_LONG && evt->value == BUTTON_RELEASED) {
        ZB_SCHEDULE_APP_ALARM_CANCEL(continous_press_timer, ZB_ALARM_ANY_PARAM);
    }

    send_scene(scene_id);
}

INPUT_CALLBACK_DEFINE(NULL, button_handler, NULL);
