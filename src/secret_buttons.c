#include <zephyr/logging/log.h>
#include <zephyr/input/input.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/retained_mem.h>
#include <zboss_api.h>
#include <ram_pwrdn.h>


LOG_MODULE_REGISTER(system, LOG_LEVEL_INF);

const static struct device *gpregret_device = DEVICE_DT_GET(DT_NODELABEL(gpregret1));

static void secret_button_handler(struct input_event *evt, void *user_data)
{
    static bool first_reset_pressed = false;
    static bool second_reset_pressed = false;
    static bool first_dfu_pressed = false;
    static bool second_dfu_pressed = false;
    if (evt->type == INPUT_EV_KEY && evt->code == 0xF3) {
        first_reset_pressed = evt->value;
    }
    if (evt->type == INPUT_EV_KEY && evt->code == 0xF4) {
        second_reset_pressed = evt->value;
    }
    if (first_reset_pressed && second_reset_pressed) {
        LOG_INF("Reseting network configuration");
        ZB_SCHEDULE_APP_CALLBACK(zb_bdb_reset_via_local_action, 0);
        return;
    }
    if (evt->type == INPUT_EV_KEY && evt->code == 0xF1) {
        first_dfu_pressed = evt->value;
    }
    if (evt->type == INPUT_EV_KEY && evt->code == 0xF2) {
        second_dfu_pressed = evt->value;
    }
    if (first_dfu_pressed && second_dfu_pressed) {
        LOG_INF("Rebooting into DFU");
        uint8_t reboot_to_dfu = 0x57;
        int err = retained_mem_write(gpregret_device, 0, &reboot_to_dfu, sizeof(reboot_to_dfu));
        if (err != 0) {
            LOG_ERR("Could not write gpregret (%d)", err);
            return;
        }
        power_up_unused_ram();
        sys_reboot(SYS_REBOOT_COLD);
        return;
    }
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(very_longpress)), secret_button_handler, NULL);
