#include <zephyr/logging/log.h>
#include <zephyr/input/input.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/retained_mem.h>
#include <zboss_api.h>
#include <ram_pwrdn.h>
#include "scene_codes.h"


LOG_MODULE_REGISTER(system, LOG_LEVEL_INF);

#define DFU_BUTTON_A_CODE       SCENE_CODE(SCENE_TYPE_VERY_LONG, 1)
#define DFU_BUTTON_B_CODE       SCENE_CODE(SCENE_TYPE_VERY_LONG, 2)
#define RESET_BUTTON_A_CODE     SCENE_CODE(SCENE_TYPE_VERY_LONG, 3)
#define RESET_BUTTON_B_CODE     SCENE_CODE(SCENE_TYPE_VERY_LONG, 4)

#define ALL_CHANNELS_MASK       0x07FFF800  /* 2.4 GHz channels 11-26 */

/* bootloader "stay in DFU" value */
#if defined(CONFIG_BOARD_NRF52840DONGLE)
#define GPREGRET_DFU_MAGIC      0xB1        /* BOOTLOADER_DFU_START */
#elif defined(CONFIG_BOARD_PROMICRO_NRF52840)
#define GPREGRET_DFU_MAGIC      0x57        /* DFU_MAGIC_UF2_RESET */
#else
#error "Unknown bootloader for this board - add its GPREGRET DFU magic"
#endif

const static struct device *gpregret_device = DEVICE_DT_GET(DT_NODELABEL(gpregret1));

static void reset_network(zb_uint8_t param)
{
    zb_set_bdb_primary_channel_set(ALL_CHANNELS_MASK);
    zb_set_bdb_secondary_channel_set(ALL_CHANNELS_MASK);
    zb_set_channel_mask(ALL_CHANNELS_MASK);
    zb_bdb_reset_via_local_action(param);
}

static void secret_button_handler(struct input_event *evt, void *user_data)
{
    static bool first_reset_pressed = false;
    static bool second_reset_pressed = false;
    static bool first_dfu_pressed = false;
    static bool second_dfu_pressed = false;
    static bool reset_triggered = false;
    static bool dfu_triggered = false;
    if (evt->type == INPUT_EV_KEY && evt->code == RESET_BUTTON_A_CODE) {
        first_reset_pressed = evt->value;
    }
    if (evt->type == INPUT_EV_KEY && evt->code == RESET_BUTTON_B_CODE) {
        second_reset_pressed = evt->value;
    }
    if (first_reset_pressed && second_reset_pressed) {
        if (reset_triggered) {
            return;
        }
        reset_triggered = true;
        LOG_INF("Reseting network configuration");
        ZB_SCHEDULE_APP_CALLBACK(reset_network, 0);
        return;
    }
    reset_triggered = false;
    if (evt->type == INPUT_EV_KEY && evt->code == DFU_BUTTON_A_CODE) {
        first_dfu_pressed = evt->value;
    }
    if (evt->type == INPUT_EV_KEY && evt->code == DFU_BUTTON_B_CODE) {
        second_dfu_pressed = evt->value;
    }
    if (first_dfu_pressed && second_dfu_pressed) {
        if (dfu_triggered) {
            return;
        }
        dfu_triggered = true;
        LOG_INF("Rebooting into DFU");
        uint8_t reboot_to_dfu = GPREGRET_DFU_MAGIC;
        int err = retained_mem_write(gpregret_device, 0, &reboot_to_dfu, sizeof(reboot_to_dfu));
        if (err != 0) {
            LOG_ERR("Could not write gpregret (%d)", err);
            return;
        }
        power_up_unused_ram();
        sys_reboot(SYS_REBOOT_COLD);
        return;
    }
    dfu_triggered = false;
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(very_longpress)), secret_button_handler, NULL);
