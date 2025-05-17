#include <zephyr/input/input.h>
#include <zephyr/sys/reboot.h>
#include <zboss_api.h>
#include <ram_pwrdn.h>

static void secret_button_handler(struct input_event *evt, void *user_data)
{
    static bool first_reset_pressed = false;
    static bool second_reset_pressed = false;
    if (evt->type == INPUT_EV_KEY && evt->code == 0xF3) {
        first_reset_pressed = evt->value;
    }
    if (evt->type == INPUT_EV_KEY && evt->code == 0xF4) {
        second_reset_pressed = evt->value;
    }
    if (first_reset_pressed && second_reset_pressed) {
        ZB_SCHEDULE_APP_CALLBACK(zb_bdb_reset_via_local_action, 0);
        return;
    }
    if (evt->type == INPUT_EV_KEY && evt->code == 0xF1 && evt->value == 1) {
        NRF_POWER->GPREGRET = 0x57;
        power_up_unused_ram();
        sys_reboot(SYS_REBOOT_COLD);
        return;
    }
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(very_longpress)), secret_button_handler, NULL);
