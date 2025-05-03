/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 * @brief Dimmer switch for HA profile implementation.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include <ram_pwrdn.h>

#define ZB_HA_DEFINE_DEVICE_SCENE_SELECTOR

#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <ha/zb_ha_scene_selector.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>
#include <zb_nrf_platform.h>
#include "zb_mem_config_custom.h"
#include "zb_dimmer_switch.h"

#define BUTTON_SCENE_1              DK_BTN1_MSK  /* Scene 1 activation */
#define BUTTON_SCENE_2              DK_BTN2_MSK  /* Scene 2 activation */
#define BUTTON_SCENE_3              DK_BTN3_MSK  /* Scene 3 activation */
#define BUTTON_SCENE_4              DK_BTN4_MSK  /* Scene 4 activation */
/* Scene IDs */
#define SCENE_1_ID                  0x01
#define SCENE_2_ID                  0x02
#define SCENE_3_ID                  0x03
#define SCENE_4_ID                  0x04

/* Source endpoint used to control light bulb. */
#define LIGHT_SWITCH_ENDPOINT      1
#define DEVICE_ENDPOINT      1
/* Delay between the light switch startup and light bulb finding procedure. */
#define MATCH_DESC_REQ_START_DELAY K_SECONDS(2)
/* Timeout for finding procedure. */
#define MATCH_DESC_REQ_TIMEOUT     K_SECONDS(5)
/* Find only non-sleepy device. */
#define MATCH_DESC_REQ_ROLE        ZB_NWK_BROADCAST_RX_ON_WHEN_IDLE

/* Do not erase NVRAM to save the network parameters after device reboot or
 * power-off. NOTE: If this option is set to ZB_TRUE then do full device erase
 * for all network devices before running other samples.
 */
#define ERASE_PERSISTENT_CONFIG    ZB_FALSE
/* LED indicating that light switch successfully joind Zigbee network. */
#define ZIGBEE_NETWORK_STATE_LED   DK_LED3
/* LED used for device identification. */
#define IDENTIFY_LED               ZIGBEE_NETWORK_STATE_LED
/* LED indicating that light witch found a light bulb to control. */
#define BULB_FOUND_LED             DK_LED4
/* Button ID used to switch on the light bulb. */
#define BUTTON_ON                  DK_BTN1_MSK
/* Button ID used to switch off the light bulb. */
#define BUTTON_OFF                 DK_BTN2_MSK
/* Dim step size - increases/decreses current level (range 0x000 - 0xfe). */
#define DIMM_STEP                  15
/* Button ID used to enable sleepy behavior. */
//#define BUTTON_SLEEPY              DK_BTN3_MSK

/* Button to start Factory Reset */
#define FACTORY_RESET_BUTTON       DK_BTN4_MSK

/* Button used to enter the Identify mode. */
#define IDENTIFY_MODE_BUTTON       DK_BTN4_MSK

/* Transition time for a single step operation in 0.1 sec units.
 * 0xFFFF - immediate change.
 */
#define DIMM_TRANSACTION_TIME      2

/* Time after which the button state is checked again to detect button hold,
 * the dimm command is sent again.
 */
#define BUTTON_LONG_POLL_TMO       K_MSEC(500)

#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE to compile light switch (End Device) source code.
#endif

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

struct bulb_context {
    zb_uint8_t endpoint;
    zb_uint16_t short_addr;
    struct k_timer find_alarm;
};

struct buttons_context {
    uint32_t state;
    atomic_t long_poll;
    struct k_timer alarm;
};

struct zb_device_ctx {
    zb_zcl_identify_attrs_t identify_attr;
};

static struct bulb_context bulb_ctx;
static struct buttons_context buttons_ctx;
static struct zb_device_ctx dev_ctx;

/* Basic cluster attributes data */
zb_uint8_t g_attr_basic_zcl_version = ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE;
zb_uint8_t g_attr_basic_application_version = ZB_ZCL_BASIC_APPLICATION_VERSION_DEFAULT_VALUE;
zb_uint8_t g_attr_basic_stack_version = ZB_ZCL_BASIC_STACK_VERSION_DEFAULT_VALUE;
zb_uint8_t g_attr_basic_hw_version = ZB_ZCL_BASIC_HW_VERSION_DEFAULT_VALUE;
zb_char_t g_attr_basic_manufacturer_name[] = ZB_ZCL_BASIC_MANUFACTURER_NAME_DEFAULT_VALUE;
zb_char_t g_attr_basic_model_identifier[] = ZB_ZCL_BASIC_MODEL_IDENTIFIER_DEFAULT_VALUE;
zb_char_t g_attr_basic_date_code[] = ZB_ZCL_BASIC_DATE_CODE_DEFAULT_VALUE;
zb_uint8_t g_attr_basic_power_source = ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE;
zb_char_t g_attr_basic_location_description[] = ZB_ZCL_BASIC_LOCATION_DESCRIPTION_DEFAULT_VALUE;
zb_uint8_t g_attr_basic_physical_environment = ZB_ZCL_BASIC_PHYSICAL_ENVIRONMENT_DEFAULT_VALUE;
zb_char_t g_attr_sw_build_id[] = ZB_ZCL_BASIC_SW_BUILD_ID_DEFAULT_VALUE;
 
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

/********************* Declare device **************************/
ZB_HA_DECLARE_ON_OFF_SWITCH_CLUSTER_LIST(on_off_switch_clusters, on_off_switch_configuration_attr_list, basic_attr_list, identify_attr_list);
ZB_HA_DECLARE_ON_OFF_SWITCH_EP(on_off_switch_ep, DEVICE_ENDPOINT, on_off_switch_clusters);
ZB_HA_DECLARE_ON_OFF_SWITCH_CTX(device_ctx, on_off_switch_ep);

/* Forward declarations. */
static void light_switch_button_handler(struct k_timer *timer);
static void find_light_bulb_alarm(struct k_timer *timer);
static void find_light_bulb(zb_bufid_t bufid);
static void light_switch_send_on_off(zb_bufid_t bufid, zb_uint16_t on_off);


/**@brief Starts identifying the device.
 *
 * @param  bufid  Unused parameter, required by ZBOSS scheduler API.
 */
static void start_identifying(zb_bufid_t bufid)
{
    ZVUNUSED(bufid);

    if (ZB_JOINED()) {
        /* Check if endpoint is in identifying mode,
         * if not, put desired endpoint in identifying mode.
         */
        if (dev_ctx.identify_attr.identify_time ==
            ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE) {

            zb_ret_t zb_err_code = zb_bdb_finding_binding_target(LIGHT_SWITCH_ENDPOINT);

            if (zb_err_code == RET_OK) {
                LOG_INF("Enter identify mode");
            } else if (zb_err_code == RET_INVALID_STATE) {
                LOG_WRN("RET_INVALID_STATE - Cannot enter identify mode");
            } else {
                ZB_ERROR_CHECK(zb_err_code);
            }
        } else {
            LOG_INF("Cancel identify mode");
            zb_bdb_finding_binding_target_cancel();
        }
    } else {
        LOG_WRN("Device not in a network - cannot enter identify mode");
    }
}

static void on_off_callback(zb_bufid_t buffer)
{
    zb_uint8_t status = zb_buf_get_status(buffer);

    LOG_INF("on_off_callback %d", buffer);

    LOG_INF("buffer %d", buffer);
    LOG_INF("status %d", status);

    /* Free the buffer */
    zb_buf_free(buffer);
}

/**@brief Callback for button events.
 *
 * @param[in]   button_state  Bitmask containing buttons state.
 * @param[in]   has_changed   Bitmask containing buttons that has
 *                            changed their state.
 */
static void button_handler(uint32_t button_state, uint32_t has_changed)
{
    uint16_t coordinator_addr = 0x0000;
    zb_uint8_t scene_id = 0;

    /* Inform default signal handler about user input at the device. */
    user_input_indicate();

    check_factory_reset_button(button_state, has_changed);

    /* Only handle button press events, not releases */
    if ((has_changed & button_state) == 0) {
        return;
    }

    /* Determine which button was pressed and send corresponding scene command */
    if (has_changed & BUTTON_SCENE_1) {
        scene_id = SCENE_1_ID;
        LOG_INF("Activating Scene 1");
    } else if (has_changed & BUTTON_SCENE_2) {
        scene_id = SCENE_2_ID;
        LOG_INF("Activating Scene 2");
    } else if (has_changed & BUTTON_SCENE_3) {
        scene_id = SCENE_3_ID;
        LOG_INF("Activating Scene 3");
    } else if (has_changed & BUTTON_SCENE_4) {
        scene_id = SCENE_4_ID;
        LOG_INF("Activating Scene 4");
    } else {
        return; // No relevant button pressed
    }

    /* If we have a valid scene ID, activate the scene */
    if (scene_id > 0) {
        /* Allocate a buffer for the scene recall command */
        zb_bufid_t bufid = zb_buf_get_out();

        if (!bufid) {
            LOG_ERR("Failed to allocate buffer for scene recall");
            return;
        }

        /* Send the scene recall command */
        ZB_ZCL_ON_OFF_SEND_REQ(
            bufid,
            coordinator_addr,
            ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            1,
            DEVICE_ENDPOINT,
            ZB_AF_HA_PROFILE_ID,
            ZB_ZCL_DISABLE_DEFAULT_RESPONSE,
            ZB_ZCL_CMD_ON_OFF_ON_ID,
            on_off_callback)
        LOG_INF("sent on off %d", bufid);
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

static void alarm_timers_init(void)
{
    k_timer_init(&buttons_ctx.alarm, light_switch_button_handler, NULL);
    k_timer_init(&bulb_ctx.find_alarm, find_light_bulb_alarm, NULL);
}

/**@brief Function for initializing all clusters attributes. */
static void app_clusters_attr_init(void)
{
    /* Identify cluster attributes data. */
    dev_ctx.identify_attr.identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;
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
        /* Schedule a self-scheduling function that will toggle the LED. */
        ZB_SCHEDULE_APP_CALLBACK(toggle_identify_led, bufid);
    } else {
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

/**@brief Function for sending ON/OFF requests to the light bulb.
 *
 * @param[in]   bufid    Non-zero reference to Zigbee stack buffer that will be
 *                       used to construct on/off request.
 * @param[in]   cmd_id   ZCL command id.
 */
static void light_switch_send_on_off(zb_bufid_t bufid, zb_uint16_t cmd_id)
{
    LOG_INF("Send ON/OFF command: %d", cmd_id);

    ZB_ZCL_ON_OFF_SEND_REQ(bufid,
                   bulb_ctx.short_addr,
                   ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
                   bulb_ctx.endpoint,
                   LIGHT_SWITCH_ENDPOINT,
                   ZB_AF_HA_PROFILE_ID,
                   ZB_ZCL_DISABLE_DEFAULT_RESPONSE,
                   cmd_id,
                   NULL);
}

/**@brief Function for sending step requests to the light bulb.
 *
 * @param[in]   bufid        Non-zero reference to Zigbee stack buffer that
 *                           will be used to construct step request.
 * @param[in]   cmd_id       ZCL command id.
 */
static void light_switch_send_step(zb_bufid_t bufid, zb_uint16_t cmd_id)
{
    LOG_INF("Send step level command: %d", cmd_id);

    ZB_ZCL_LEVEL_CONTROL_SEND_STEP_REQ(bufid,
                       bulb_ctx.short_addr,
                       ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
                       bulb_ctx.endpoint,
                       LIGHT_SWITCH_ENDPOINT,
                       ZB_AF_HA_PROFILE_ID,
                       ZB_ZCL_DISABLE_DEFAULT_RESPONSE,
                       NULL,
                       cmd_id,
                       DIMM_STEP,
                       DIMM_TRANSACTION_TIME);
}

/**@brief Callback function receiving finding procedure results.
 *
 * @param[in]   bufid   Reference to Zigbee stack buffer used to pass
 *                      received data.
 */
static void find_light_bulb_cb(zb_bufid_t bufid)
{
    /* Get the beginning of the response. */
    zb_zdo_match_desc_resp_t *resp =
        (zb_zdo_match_desc_resp_t *) zb_buf_begin(bufid);
    /* Get the pointer to the parameters buffer, which stores APS layer
     * response.
     */
    zb_apsde_data_indication_t *ind = ZB_BUF_GET_PARAM(bufid,
                               zb_apsde_data_indication_t);
    zb_uint8_t *match_ep;

    if ((resp->status == ZB_ZDP_STATUS_SUCCESS) &&
        (resp->match_len > 0) &&
        (bulb_ctx.short_addr == 0xFFFF)) {

        /* Match EP list follows right after response header. */
        match_ep = (zb_uint8_t *)(resp + 1);

        /* We are searching for exact cluster, so only 1 EP
         * may be found.
         */
        bulb_ctx.endpoint = *match_ep;
        bulb_ctx.short_addr = ind->src_addr;

        LOG_INF("Found bulb addr: %d ep: %d",
            bulb_ctx.short_addr,
            bulb_ctx.endpoint);

        k_timer_stop(&bulb_ctx.find_alarm);
        dk_set_led_on(BULB_FOUND_LED);
    } else {
        LOG_INF("Bulb not found, try again");
    }

    if (bufid) {
        zb_buf_free(bufid);
    }
}

/**@brief Find bulb allarm handler.
 *
 * @param[in]   timer   Address of timer.
 */
static void find_light_bulb_alarm(struct k_timer *timer)
{
    ZB_ERROR_CHECK(zb_buf_get_out_delayed(find_light_bulb));
}

/**@brief Function for sending ON/OFF and Level Control find request.
 *
 * @param[in]   bufid   Reference to Zigbee stack buffer that will be used to
 *                      construct find request.
 */
static void find_light_bulb(zb_bufid_t bufid)
{
    zb_zdo_match_desc_param_t *req;
    zb_uint8_t tsn = ZB_ZDO_INVALID_TSN;

    /* Initialize pointers inside buffer and reserve space for
     * zb_zdo_match_desc_param_t request.
     */
    req = zb_buf_initial_alloc(bufid,
                   sizeof(zb_zdo_match_desc_param_t) + (1) * sizeof(zb_uint16_t));

    req->nwk_addr = MATCH_DESC_REQ_ROLE;
    req->addr_of_interest = MATCH_DESC_REQ_ROLE;
    req->profile_id = ZB_AF_HA_PROFILE_ID;

    /* We are searching for 2 clusters: On/Off and Level Control Server. */
    req->num_in_clusters = 2;
    req->num_out_clusters = 0;
    req->cluster_list[0] = ZB_ZCL_CLUSTER_ID_ON_OFF;
    req->cluster_list[1] = ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL;

    /* Set 0xFFFF to reset short address in order to parse
     * only one response.
     */
    bulb_ctx.short_addr = 0xFFFF;
    tsn = zb_zdo_match_desc_req(bufid, find_light_bulb_cb);

    /* Free buffer if failed to send a request. */
    if (tsn == ZB_ZDO_INVALID_TSN) {
        zb_buf_free(bufid);

        LOG_ERR("Failed to send Match Descriptor request");
    }
}

/**@brief Callback for detecting button press duration.
 *
 * @param[in]   timer   Address of timer.
 */
static void light_switch_button_handler(struct k_timer *timer)
{
    zb_ret_t zb_err_code;
    zb_uint16_t cmd_id;

    if (dk_get_buttons() & buttons_ctx.state) {
        atomic_set(&buttons_ctx.long_poll, ZB_TRUE);
        if (buttons_ctx.state == BUTTON_ON) {
            cmd_id = ZB_ZCL_LEVEL_CONTROL_STEP_MODE_UP;
        } else {
            cmd_id = ZB_ZCL_LEVEL_CONTROL_STEP_MODE_DOWN;
        }

        /* Allocate output buffer and send step command. */
        zb_err_code = zb_buf_get_out_delayed_ext(light_switch_send_step,
                             cmd_id,
                             0);
        if (!zb_err_code) {
            LOG_WRN("Buffer is full");
        }

        k_timer_start(&buttons_ctx.alarm, BUTTON_LONG_POLL_TMO,
                  K_NO_WAIT);
    } else {
        atomic_set(&buttons_ctx.long_poll, ZB_FALSE);
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
    zb_ret_t status = ZB_GET_APP_SIGNAL_STATUS(bufid);

    /* Update network status LED. */
    zigbee_led_status_update(bufid, ZIGBEE_NETWORK_STATE_LED);

    switch (sig) {
    case ZB_BDB_SIGNAL_DEVICE_REBOOT:
    /* fall-through */
    case ZB_BDB_SIGNAL_STEERING:
        /* Call default signal handler. */
        ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));
        if (status == RET_OK) {
            /* Check the light device address. */
            if (bulb_ctx.short_addr == 0xFFFF) {
                k_timer_start(&bulb_ctx.find_alarm,
                          MATCH_DESC_REQ_START_DELAY,
                          MATCH_DESC_REQ_TIMEOUT);
            }
        }
        break;
    case ZB_ZDO_SIGNAL_LEAVE:
        /* If device leaves the network, reset bulb short_addr. */
        if (status == RET_OK) {
            zb_zdo_signal_leave_params_t *leave_params =
                ZB_ZDO_SIGNAL_GET_PARAMS(sig_hndler, zb_zdo_signal_leave_params_t);

            if (leave_params->leave_type == ZB_NWK_LEAVE_TYPE_RESET) {
                bulb_ctx.short_addr = 0xFFFF;
            }
        }
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
    //alarm_timers_init();
    //register_factory_reset_button(FACTORY_RESET_BUTTON);

    //TODO sprawdzić z true
    zigbee_erase_persistent_storage(ERASE_PERSISTENT_CONFIG);
    //zb_set_ed_timeout(ED_AGING_TIMEOUT_64MIN);
    //zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(3000));

    /* Set default bulb short_addr. */
    //bulb_ctx.short_addr = 0xFFFF;

    /* If "sleepy button" is defined, check its state during Zigbee
     * initialization and enable sleepy behavior at device if defined button
     * is pressed.
     */
#if defined BUTTON_SLEEPY
    //if (dk_get_buttons() & BUTTON_SLEEPY) {
    //    zigbee_configure_sleepy_behavior(true);
    //}
#endif

    /* Power off unused sections of RAM to lower device power consumption. */
    //if (IS_ENABLED(CONFIG_RAM_POWER_DOWN_LIBRARY)) {
    //    power_down_unused_ram();
    //}

    /* Register device context (endpoints). */
    ZB_AF_REGISTER_DEVICE_CTX(&device_ctx);

    //app_clusters_attr_init();

    /* Register handlers to identify notifications */
    //ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(LIGHT_SWITCH_ENDPOINT, identify_cb);
    //ZB_AF_SET_ENDPOINT_HANDLER(SCENE_SELECTOR_ENDPOINT, zcl_specific_cluster_cmd_handler);

    /* Start Zigbee default thread. */
    zigbee_enable();

    LOG_INF("Zigbee R23 Light Switch example started");

    while (1) {
        k_sleep(K_FOREVER);
    }
}
