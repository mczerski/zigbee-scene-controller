/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 * @brief Dimmer switch for HA profile implementation.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include <ram_pwrdn.h>

#include <zboss_api.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>
#include <zb_nrf_platform.h>
#include "my_device.h"

#define BUTTON_MSK                 DK_BTN1_MSK  /* Scene 1 activation */
#define MY_DEVICE_ENDPOINT         1

/* Do not erase NVRAM to save the network parameters after device reboot or
 * power-off. NOTE: If this option is set to ZB_TRUE then do full device erase
 * for all network devices before running other samples.
 */
#define ERASE_PERSISTENT_CONFIG    ZB_FALSE
/* LED indicating that light switch successfully joind Zigbee network. */
#define ZIGBEE_NETWORK_STATE_LED   DK_LED3
/* LED used for device identification. */
#define IDENTIFY_LED               ZIGBEE_NETWORK_STATE_LED
/* Button ID used to enable sleepy behavior. */
#define BUTTON_SLEEPY              DK_BTN1_MSK

/* Button to start Factory Reset */
#define FACTORY_RESET_BUTTON       DK_BTN1_MSK

#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE to compile light switch (End Device) source code.
#endif

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

struct buttons_context {
    int state;
    struct k_timer press_timer;
};

static struct buttons_context buttons_ctx;

/* Basic cluster attributes data */
zb_uint8_t g_attr_basic_zcl_version = ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE;
zb_uint8_t g_attr_basic_application_version = (0 << 4) | 1;
zb_uint8_t g_attr_basic_stack_version = (ZBOSS_MAJOR << 4) | ZBOSS_MINOR;
zb_uint8_t g_attr_basic_hw_version = (0 << 4) | 1;
zb_char_t g_attr_basic_manufacturer_name[] = "\x0d" "Marek Czerski";
zb_char_t g_attr_basic_model_identifier[] = "\x10" "Scene controller";
zb_char_t g_attr_basic_date_code[] = "\x08" "20250503";
zb_uint8_t g_attr_basic_power_source = ZB_ZCL_BASIC_POWER_SOURCE_BATTERY;
zb_char_t g_attr_basic_location_description[] = ZB_ZCL_BASIC_LOCATION_DESCRIPTION_DEFAULT_VALUE;
zb_uint8_t g_attr_basic_physical_environment = ZB_ZCL_BASIC_PHYSICAL_ENVIRONMENT_DEFAULT_VALUE;
zb_char_t g_attr_sw_build_id[] = "\x07" "9dff6ce";

/* Define 'bat_num' as empty in order to declare default battery set attributes. */
/* According to Table 3-17 of ZCL specification, defining 'bat_num' as 2 or 3 allows */
/* to declare battery set attributes for BATTERY2 and BATTERY3 */
#define bat_num

ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(
    basic_attr_list,
    &g_attr_basic_zcl_version,
    &g_attr_basic_application_version,
    &g_attr_basic_stack_version,
    &g_attr_basic_hw_version,
    &g_attr_basic_manufacturer_name,
    &g_attr_basic_model_identifier,
    &g_attr_basic_date_code,
    &g_attr_basic_power_source,
    &g_attr_basic_location_description,
    &g_attr_basic_physical_environment,
    &g_attr_sw_build_id
);

/* Identify cluster attributes data */
zb_uint16_t g_attr_identify_identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;
/* On/Off Switch Configuration cluster attributes data */
zb_uint8_t g_attr_on_off_switch_configuration_switch_type = 0;
zb_uint8_t g_attr_on_off_switch_configuration_switch_actions = ZB_ZCL_ON_OFF_SWITCH_CONFIGURATION_SWITCH_ACTIONS_DEFAULT_VALUE;
ZB_ZCL_DECLARE_ON_OFF_SWITCH_CONFIGURATION_ATTRIB_LIST(
    on_off_switch_configuration_attr_list,
    &g_attr_on_off_switch_configuration_switch_type,
    &g_attr_on_off_switch_configuration_switch_actions
);
ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(identify_attr_list, &g_attr_identify_identify_time);

/* Power configuration cluster attributes data */
zb_uint8_t g_attr_battery_voltage = 3900 / 100; //100mV unit //ZB_ZCL_POWER_CONFIG_BATTERY_VOLTAGE_INVALID;
zb_uint8_t g_attr_battery_size = ZB_ZCL_POWER_CONFIG_BATTERY_SIZE_BUILT_IN;//ZB_ZCL_POWER_CONFIG_BATTERY_SIZE_DEFAULT_VALUE;
zb_uint8_t g_attr_battery_quantity = 1;
zb_uint8_t g_attr_battery_rated_voltage = 3700 / 100; // 100mV unit
zb_uint8_t g_attr_battery_alarm_mask = ZB_ZCL_POWER_CONFIG_BATTERY_ALARM_MASK_DEFAULT_VALUE;
zb_uint8_t g_attr_battery_voltage_min_threshold = ZB_ZCL_POWER_CONFIG_BATTERY_VOLTAGE_MIN_THRESHOLD_DEFAULT_VALUE;
zb_uint8_t g_attr_battery_percentage_remaining = 85*2;// 0.5% unit //ZB_ZCL_POWER_CONFIG_BATTERY_REMAINING_UNKNOWN;
zb_uint8_t g_attr_battery_voltage_threshold1 = ZB_ZCL_POWER_CONFIG_BATTERY_VOLTAGE_THRESHOLD1_DEFAULT_VALUE;
zb_uint8_t g_attr_battery_voltage_threshold2 = ZB_ZCL_POWER_CONFIG_BATTERY_VOLTAGE_THRESHOLD2_DEFAULT_VALUE;
zb_uint8_t g_attr_battery_voltage_threshold3 = ZB_ZCL_POWER_CONFIG_BATTERY_VOLTAGE_THRESHOLD3_DEFAULT_VALUE;
zb_uint8_t g_attr_battery_percentage_min_threshold = ZB_ZCL_POWER_CONFIG_BATTERY_PERCENTAGE_MIN_THRESHOLD_DEFAULT_VALUE;
zb_uint8_t g_attr_battery_percentage_threshold1 = ZB_ZCL_POWER_CONFIG_BATTERY_PERCENTAGE_THRESHOLD1_DEFAULT_VALUE;
zb_uint8_t g_attr_battery_percentage_threshold2 = ZB_ZCL_POWER_CONFIG_BATTERY_PERCENTAGE_THRESHOLD2_DEFAULT_VALUE;
zb_uint8_t g_attr_battery_percentage_threshold3 = ZB_ZCL_POWER_CONFIG_BATTERY_PERCENTAGE_THRESHOLD3_DEFAULT_VALUE;
zb_uint32_t g_attr_battery_alarm_state = ZB_ZCL_POWER_CONFIG_BATTERY_ALARM_STATE_DEFAULT_VALUE;

ZB_ZCL_DECLARE_POWER_CONFIG_BATTERY_ATTRIB_LIST_EXT(
    power_config_attr_list,
    &g_attr_battery_voltage,
    &g_attr_battery_size,
    &g_attr_battery_quantity,
    &g_attr_battery_rated_voltage,
    &g_attr_battery_alarm_mask,
    &g_attr_battery_voltage_min_threshold,
    &g_attr_battery_percentage_remaining,
    &g_attr_battery_voltage_threshold1,
    &g_attr_battery_voltage_threshold2,
    &g_attr_battery_voltage_threshold3,
    &g_attr_battery_percentage_min_threshold,
    &g_attr_battery_percentage_threshold1,
    &g_attr_battery_percentage_threshold2,
    &g_attr_battery_percentage_threshold3,
    &g_attr_battery_alarm_state
);

/********************* Declare device **************************/
ZB_HA_DECLARE_MY_DEVICE_CLUSTER_LIST(my_device_clusters, on_off_switch_configuration_attr_list, basic_attr_list, identify_attr_list, power_config_attr_list);
ZB_HA_DECLARE_MY_DEVICE_EP(my_device_ep, MY_DEVICE_ENDPOINT, my_device_clusters);
ZB_HA_DECLARE_MY_DEVICE_CTX(device_ctx, my_device_ep);

static void on_off_callback(zb_bufid_t buffer)
{
    zb_uint8_t status = zb_buf_get_status(buffer);

    LOG_INF("on_off_callback %d", buffer);

    LOG_INF("buffer %d", buffer);
    LOG_INF("status %d", status);

    /* Free the buffer */
    zb_buf_free(buffer);
}

static void send_on_off(zb_bufid_t bufid, zb_uint16_t cmd_id)
{
    uint16_t addr = 0x0000;
    ZB_ZCL_ON_OFF_SEND_REQ(
        bufid,
        addr,
        ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        1,
        MY_DEVICE_ENDPOINT,
        ZB_AF_HA_PROFILE_ID,
        ZB_ZCL_DISABLE_DEFAULT_RESPONSE,
        cmd_id,
        on_off_callback
    )
    LOG_INF("Sent On/Off command: %d", cmd_id);
    //ZB_ZCL_SCENES_SEND_RECALL_SCENE_REQ(
    //    bufid,
    //    addr,
    //    ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
    //    1,
    //    MY_DEVICE_ENDPOINT,
    //    ZB_AF_HA_PROFILE_ID,
    //    ZB_ZCL_DISABLE_DEFAULT_RESPONSE,
    //    on_off_callback,
    //    0, // group id
    //    cmd_id
    //)
    //LOG_INF("Sent scene command: %d", cmd_id);
}

/**@brief Callback for button events.
 *
 * @param[in]   button_state  Bitmask containing buttons state.
 * @param[in]   has_changed   Bitmask containing buttons that has
 *                            changed their state.
 */
static void button_handler(uint32_t button_state, uint32_t has_changed)
{
    /* Inform default signal handler about user input at the device. */
    user_input_indicate();

    check_factory_reset_button(button_state, has_changed);

    if (has_changed & BUTTON_MSK) {
        if (button_state & BUTTON_MSK) {
            LOG_INF("Button pressed");
            buttons_ctx.state = 0;
            k_timer_start(&buttons_ctx.press_timer, K_MSEC(500), K_NO_WAIT);
        }
        else if (!was_factory_reset_done()) {
            LOG_INF("Button released");
            k_timer_stop(&buttons_ctx.press_timer);
            zb_uint16_t cmd_id = ZB_ZCL_CMD_ON_OFF_ON_ID;
            if (buttons_ctx.state == 1) {
                cmd_id = ZB_ZCL_CMD_ON_OFF_OFF_ID;
            }
            else if (buttons_ctx.state == 2) {
                cmd_id = ZB_ZCL_CMD_ON_OFF_TOGGLE_ID;
            }
            zb_ret_t zb_err_code = zb_buf_get_out_delayed_ext(send_on_off, cmd_id, 0);
            if (zb_err_code) {
                LOG_WRN("Buffer is full");
            }
        }
    }
}

static void button_timer(struct k_timer *timer)
{
    if (dk_get_buttons() & BUTTON_MSK) {
        buttons_ctx.state += 1;
        k_timer_start(&buttons_ctx.press_timer, K_MSEC(1500), K_NO_WAIT);
    }
}

/**@brief Function for initializing LEDs and Buttons. */
static void configure_gpio(void)
{
    int err;

    err = dk_buttons_init(button_handler);
    if (err) {
        LOG_ERR("Cannot init buttons (err: %d)", err);
    }

    err = dk_leds_init();
    if (err) {
        LOG_ERR("Cannot init LEDs (err: %d)", err);
    }
}

static void configure_timers(void)
{
    k_timer_init(&buttons_ctx.press_timer, button_timer, NULL);
}

/**@brief Function to toggle the identify LED.
 *
 * @param  bufid  Unused parameter, required by ZBOSS scheduler API.
 */
static void toggle_identify_led(zb_bufid_t bufid)
{
    static int blink_status;

    dk_set_led(IDENTIFY_LED, (++blink_status) % 2);
    ZB_SCHEDULE_APP_ALARM(toggle_identify_led, bufid, ZB_MILLISECONDS_TO_BEACON_INTERVAL(100));
}

/**@brief Function to handle identify notification events on the first endpoint.
 *
 * @param  bufid  Unused parameter, required by ZBOSS scheduler API.
 */
static void identify_cb(zb_bufid_t bufid)
{
    zb_ret_t zb_err_code;

    if (bufid) {
        LOG_INF("Identify started");
        /* Schedule a self-scheduling function that will toggle the LED. */
        ZB_SCHEDULE_APP_CALLBACK(toggle_identify_led, bufid);
    } else {
        LOG_INF("Identify stopped");
        /* Cancel the toggling function alarm and turn off LED. */
        zb_err_code = ZB_SCHEDULE_APP_ALARM_CANCEL(toggle_identify_led, ZB_ALARM_ANY_PARAM);
        ZVUNUSED(zb_err_code);

        /* Update network status/idenitfication LED. */
        if (ZB_JOINED()) {
            dk_set_led_on(ZIGBEE_NETWORK_STATE_LED);
        } else {
            dk_set_led_off(ZIGBEE_NETWORK_STATE_LED);
        }
    }
}

/**@brief Zigbee stack event handler.
 *
 * @param[in]   bufid   Reference to the Zigbee stack buffer
 *                      used to pass signal.
 */
void zboss_signal_handler(zb_bufid_t bufid)
{
    zb_zdo_app_signal_hdr_t *sig_hndler = NULL;
    zb_zdo_app_signal_type_t sig = zb_get_app_signal(bufid, &sig_hndler);

    /* Update network status LED. */
    zigbee_led_status_update(bufid, ZIGBEE_NETWORK_STATE_LED);

    switch (sig) {
    case ZB_BDB_SIGNAL_DEVICE_REBOOT:
    /* fall-through */
    case ZB_BDB_SIGNAL_STEERING:
        /* Call default signal handler. */
        ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));
    case ZB_ZDO_SIGNAL_LEAVE:
        /* Call default signal handler. */
        ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));
        break;
    default:
        /* Call default signal handler. */
        ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));
        break;
    }

    if (bufid) {
        zb_buf_free(bufid);
    }
}

zb_uint8_t zcl_specific_cluster_cmd_handler(zb_uint8_t param)
{
    zb_zcl_parsed_hdr_t cmd_info;
    zb_uint8_t lqi = ZB_MAC_LQI_UNDEFINED;
    zb_int8_t rssi = ZB_MAC_RSSI_UNDEFINED;

    LOG_INF("> zcl_specific_cluster_cmd_handler");

    ZB_ZCL_COPY_PARSED_HEADER(param, &cmd_info);

    zb_uint16_t g_dst_addr = ZB_ZCL_PARSED_HDR_SHORT_DATA(&cmd_info).source.u.short_addr;

    ZB_ZCL_DEBUG_DUMP_HEADER(&cmd_info);
    LOG_INF("payload size: %i", zb_buf_len(param));

    zb_zdo_get_diag_data(g_dst_addr, &lqi, &rssi);
    LOG_INF("lqi %hd rssi %d", lqi, rssi);

    if (cmd_info.cmd_direction == ZB_ZCL_FRAME_DIRECTION_TO_CLI)
    {
        LOG_ERR("Unsupported \"from server\" command direction");
    }
    LOG_INF("< zcl_specific_cluster_cmd_handler");
    return ZB_FALSE;
}

int main(void)
{
    LOG_INF("Starting Zigbee R23 Light Switch example");

    /* Initialize. */
    configure_gpio();
    configure_timers();
    register_factory_reset_button(FACTORY_RESET_BUTTON);

    zigbee_erase_persistent_storage(ERASE_PERSISTENT_CONFIG);
    //zb_set_ed_timeout(ED_AGING_TIMEOUT_64MIN);
    //zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(3000));

    /* If "sleepy button" is defined, check its state during Zigbee
     * initialization and enable sleepy behavior at device if defined button
     * is pressed.
     */
#if defined BUTTON_SLEEPY
    //TODO: sparwdzić to
    if (dk_get_buttons() & BUTTON_SLEEPY) {
        zigbee_configure_sleepy_behavior(true);
    }
#endif

    /* Power off unused sections of RAM to lower device power consumption. */
    if (IS_ENABLED(CONFIG_RAM_POWER_DOWN_LIBRARY)) {
        power_down_unused_ram();
    }

    /* Register device context (endpoints). */
    ZB_AF_REGISTER_DEVICE_CTX(&device_ctx);

    /* Register handlers to identify notifications */
    ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(MY_DEVICE_ENDPOINT, identify_cb);
    //ZB_AF_SET_ENDPOINT_HANDLER(MY_DEVICE_ENDPOINT, zcl_specific_cluster_cmd_handler);

    /* Start Zigbee default thread. */
    zigbee_enable();

    LOG_INF("Zigbee R23 Light Switch example started");

    //zb_uint8_t battery_attribute = 32;
    //if (zb_zcl_set_attr_val(
    //        MY_DEVICE_ENDPOINT,
    //        ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
    //        ZB_ZCL_CLUSTER_SERVER_ROLE,
    //        ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID,
    //        &battery_attribute,
    //        ZB_FALSE
    //    ))
    //{
    //    LOG_ERR("Failed to set ZCL attribute");
    //    return 0;
    //}
    //battery_attribute = 79*2;
    //if (zb_zcl_set_attr_val(
    //        MY_DEVICE_ENDPOINT,
    //        ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
    //        ZB_ZCL_CLUSTER_SERVER_ROLE,
    //        ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID,
    //        &battery_attribute,
    //        ZB_FALSE
    //    ))
    //{
    //    LOG_ERR("Failed to set ZCL attribute");
    //    return 0;
    //}
    k_sleep(K_FOREVER);
    return 0;
}
