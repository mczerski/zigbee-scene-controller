#include <zephyr/logging/log.h>
#include <zephyr/input/input.h>
#include "zigbee.h"


LOG_MODULE_REGISTER(button, LOG_LEVEL_INF);

static void button_handler(struct input_event *evt, void *user_data)
{
    LOG_INF("Button event. type=%d, code=0x%x, value=%d", evt->type, evt->code, evt->value);

    if (evt->type != INPUT_EV_KEY) {
        return;
    }
    uint16_t scene_id = 0;

    uint16_t scene_type = evt->code >> 4;
    if (
        (scene_type == 2 && evt->value == 1) || // long press activation
        (scene_type == 3 && evt->value == 0) || // single press
        (scene_type == 4 && evt->value == 0) // double press
    ) {
        scene_id = evt->code;
    }

    send_scene(scene_id);
}

INPUT_CALLBACK_DEFINE(NULL, button_handler, NULL);
