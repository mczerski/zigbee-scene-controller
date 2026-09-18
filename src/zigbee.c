#include "led.h"
#include "battery.h"
#include "my_device.h"
#include <zephyr/logging/log.h>
#include <zboss_api.h>
#include <zigbee/zigbee_error_handler.h>
#include <zigbee/zigbee_app_utils.h>
#include <ram_pwrdn.h>
#include <zb_nrf_platform.h>


#define MY_DEVICE_ENDPOINT         1
/* Do not erase NVRAM to save the network parameters after device reboot or
 * power-off. NOTE: If this option is set to ZB_TRUE then do full device erase
 * for all network devices before running other samples.
 */
#define ERASE_PERSISTENT_CONFIG    ZB_FALSE
#define RESEND_THRESHOLD_S         10
#define JOIN_BLINK_INTERVAL_S      5
#define IDENTIFY_BLINK_MS          100
/* greater values here do not play nice with zigbee2mqtt */
#define LONG_POLL_INTERVAL_MS      900000

/* the scenes are recalled on the coordinator */
#define COORDINATOR_ADDR           0x0000
#define COORDINATOR_ENDPOINT       1
#define SCENES_GROUP_ID            0

/* solid flash on success, two short blinks on failure */
#define LED_OK_ON_MS               200
#define LED_FAIL_ON_MS             50
#define LED_FAIL_OFF_MS            200
#define LED_FAIL_COUNT             2

/* ZCL reports the voltage in 100 mV and the charge level in 0.5 % units,
 * set_battery_state() is given millivolts and decipercents */
#define ZCL_VOLTAGE_UNIT_MV        100
#define ZCL_LEVEL_UNIT_DP          5
#define BATTERY_RATED_VOLTAGE_MV   3700

LOG_MODULE_REGISTER(zigbee, LOG_LEVEL_INF);

/* Basic cluster attributes data */
// TODO: use deicated macros to prepare strings
zb_uint8_t g_attr_basic_zcl_version = ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE;
zb_uint8_t g_attr_basic_application_version = (0 << 4) | 1;
zb_uint8_t g_attr_basic_stack_version = (ZBOSS_MAJOR << 4) | ZBOSS_MINOR;
zb_uint8_t g_attr_basic_hw_version = (0 << 4) | 1;
zb_char_t g_attr_basic_manufacturer_name[] = "\x0d" "Marek Czerski";
zb_char_t g_attr_basic_model_identifier[] = "\x10" "Scene controller";
zb_char_t g_attr_basic_date_code[] = "\x08" "20260917";
zb_uint8_t g_attr_basic_power_source = ZB_ZCL_BASIC_POWER_SOURCE_BATTERY;
zb_char_t g_attr_basic_location_description[] = ZB_ZCL_BASIC_LOCATION_DESCRIPTION_DEFAULT_VALUE;
zb_uint8_t g_attr_basic_physical_environment = ZB_ZCL_BASIC_PHYSICAL_ENVIRONMENT_DEFAULT_VALUE;
// TODO: generate version with cmake
zb_char_t g_attr_sw_build_id[] = "\x07" "188a339";

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
ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(identify_attr_list, &g_attr_identify_identify_time);

/* Power configuration cluster attributes data */
zb_uint8_t g_attr_battery_voltage = ZB_ZCL_POWER_CONFIG_BATTERY_VOLTAGE_INVALID;
zb_uint8_t g_attr_battery_size = ZB_ZCL_POWER_CONFIG_BATTERY_SIZE_BUILT_IN;
zb_uint8_t g_attr_battery_quantity = 1;
zb_uint8_t g_attr_battery_rated_voltage = BATTERY_RATED_VOLTAGE_MV / ZCL_VOLTAGE_UNIT_MV;
zb_uint8_t g_attr_battery_alarm_mask = ZB_ZCL_POWER_CONFIG_BATTERY_ALARM_MASK_DEFAULT_VALUE;
zb_uint8_t g_attr_battery_voltage_min_threshold = ZB_ZCL_POWER_CONFIG_BATTERY_VOLTAGE_MIN_THRESHOLD_DEFAULT_VALUE;
zb_uint8_t g_attr_battery_percentage_remaining = ZB_ZCL_POWER_CONFIG_BATTERY_REMAINING_UNKNOWN;
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
ZB_HA_DECLARE_MY_DEVICE_CLUSTER_LIST(my_device_clusters, basic_attr_list, identify_attr_list, power_config_attr_list);
ZB_HA_DECLARE_MY_DEVICE_EP(my_device_ep, MY_DEVICE_ENDPOINT, my_device_clusters);
ZB_HA_DECLARE_MY_DEVICE_CTX(device_ctx, my_device_ep);

static zb_bool_t g_scene_failed = ZB_FALSE;
static zb_uint16_t g_scene_id = 0;
static zb_time_t g_scene_timestamp = 0;

static void scene_callback(zb_bufid_t buffer)
{
    zb_ret_t status = zb_buf_get_status(buffer);

    LOG_INF("scene_callback: buffer %d, status %d", buffer, status);

    if (status != RET_OK) {
        blink_state_led(LED_FAIL_ON_MS, LED_FAIL_OFF_MS, LED_FAIL_COUNT);
        g_scene_failed = ZB_TRUE;
    }
    else {
        blink_state_led(LED_OK_ON_MS, 0, 0);
    }
    /* Free the buffer */
    zb_buf_free(buffer);
}

static void do_send_scene(zb_bufid_t bufid, zb_uint16_t scene_id)
{
    uint16_t addr = COORDINATOR_ADDR;
    ZB_ZCL_SCENES_SEND_RECALL_SCENE_REQ(
        bufid,
        addr,
        ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        COORDINATOR_ENDPOINT,
        MY_DEVICE_ENDPOINT,
        ZB_AF_HA_PROFILE_ID,
        ZB_ZCL_DISABLE_DEFAULT_RESPONSE,
        scene_callback,
        SCENES_GROUP_ID,
        scene_id
    );
    LOG_INF("Sent scene command: 0x%x", scene_id);
}

void send_scene(uint16_t scene_id)
{
    /* Inform default signal handler about user input at the device. */
    user_input_indicate();

    if (scene_id == 0) {
        return;
    }

    g_scene_failed = ZB_FALSE;
    g_scene_id = scene_id;
    g_scene_timestamp = ZB_TIMER_GET();
    zb_ret_t zb_err_code = zb_buf_get_out_delayed_ext(do_send_scene, scene_id, 0);
    if (zb_err_code) {
        LOG_WRN("Buffer is full");
    }
}

static void join_status_led(zb_uint8_t interval_s)
{
    ZB_SCHEDULE_APP_ALARM(join_status_led, interval_s, ZB_SECONDS_TO_BEACON_INTERVAL(interval_s));
    blink_state_led(LED_FAIL_ON_MS, LED_FAIL_OFF_MS, LED_FAIL_COUNT);
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
    LOG_INF("ZBOSS signal handler, sig: %d, status: %d, joined: %d", sig, status, ZB_JOINED());

    switch (sig) {
    case ZB_BDB_SIGNAL_DEVICE_REBOOT:
    /* fall-through */
    case ZB_BDB_SIGNAL_STEERING:
        if (status == RET_OK) {
            blink_state_led(LED_OK_ON_MS, 0, 0);
            /* start battery measurement timer */
            zb_time_t battery_timeout;
            if (ZB_SCHEDULE_GET_ALARM_TIME(battery_alarm_handler, ZB_ALARM_ANY_PARAM, &battery_timeout) != RET_OK) {
                ZB_SCHEDULE_APP_ALARM(battery_alarm_handler, ZB_ALARM_ANY_PARAM, ZB_TIME_ONE_SECOND);
            }
            // TODO: greater values here do not play nice with zigbee2mqtt, should it be nogotiated or smth ? 
            zb_zdo_pim_set_long_poll_interval(LONG_POLL_INTERVAL_MS);
            uint32_t channel_mask = 1 << zb_get_current_channel();
            zb_set_bdb_primary_channel_set(channel_mask);
            zb_set_bdb_secondary_channel_set(channel_mask);
            zb_set_channel_mask(channel_mask);
        }
        else {
            off_state_led(0);
        }
        /* Call default signal handler. */
        ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));
        break;
    case ZB_ZDO_SIGNAL_LEAVE:
        off_state_led(0);
        /* Call default signal handler. */
        ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));
        break;
    case ZB_COMMON_SIGNAL_CAN_SLEEP:
        /* Call default signal handler. */
        ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));
        break;
    default:
        /* Call default signal handler. */
        ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));
        break;
    }

    if (sig != ZB_COMMON_SIGNAL_CAN_SLEEP) {
        if (ZB_JOINED()) {
            ZB_SCHEDULE_APP_ALARM_CANCEL(join_status_led, ZB_ALARM_ANY_PARAM);
            if (g_scene_failed && ZB_TIME_GE(ZB_TIMER_GET(), ZB_TIME_ADD(g_scene_timestamp, ZB_TIME_ONE_SECOND * RESEND_THRESHOLD_S))) {
                LOG_INF("Retrying scene command 0x%x", g_scene_id);
                send_scene(g_scene_id);
            }
        }
        else {
            zb_time_t join_timeout;
            if (ZB_SCHEDULE_GET_ALARM_TIME(join_status_led, JOIN_BLINK_INTERVAL_S, &join_timeout) != RET_OK) {
                ZB_SCHEDULE_APP_ALARM(join_status_led, JOIN_BLINK_INTERVAL_S, ZB_TIME_ONE_SECOND);
            }
        }
    }

    if (bufid) {
        zb_buf_free(bufid);
    }
}

/**@brief Function to handle identify notification events on the first endpoint.
 *
 * @param  bufid  Unused parameter, required by ZBOSS scheduler API.
 */
static void identify_cb(zb_bufid_t bufid)
{
    if (bufid) {
        LOG_INF("Identify started");
        blink_state_led(IDENTIFY_BLINK_MS, IDENTIFY_BLINK_MS, LED_BLINK_FOREVER);
    } else {
        LOG_INF("Identify stopped");
        off_state_led(0);
    }
}

void configure_zigbee(void)
{
    zigbee_erase_persistent_storage(ERASE_PERSISTENT_CONFIG);
    zb_set_ed_timeout(ED_AGING_TIMEOUT_256MIN);
    zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(LONG_POLL_INTERVAL_MS));
    zigbee_configure_sleepy_behavior(true);
    power_down_unused_ram();

    /* Register device context (endpoints). */
    ZB_AF_REGISTER_DEVICE_CTX(&device_ctx);

    /* Register handlers to identify notifications */
    ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(MY_DEVICE_ENDPOINT, identify_cb);

    /* Start Zigbee default thread. */
    zigbee_enable();
}

void set_battery_state(int32_t battery_voltage_mv, int32_t battery_level_dp)
{
    // milli volts to decy volts
    zb_uint8_t battery_attribute = battery_voltage_mv / ZCL_VOLTAGE_UNIT_MV;
    if (zb_zcl_set_attr_val(
            MY_DEVICE_ENDPOINT,
            ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
            ZB_ZCL_CLUSTER_SERVER_ROLE,
            ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID,
            &battery_attribute,
            ZB_FALSE
        ))
    {
        LOG_ERR("Failed to set ZCL attribute");
    }
    // decy percents to half percents
    battery_attribute = battery_level_dp / ZCL_LEVEL_UNIT_DP;
    if (zb_zcl_set_attr_val(
            MY_DEVICE_ENDPOINT,
            ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
            ZB_ZCL_CLUSTER_SERVER_ROLE,
            ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID,
            &battery_attribute,
            ZB_FALSE
        ))
    {
        LOG_ERR("Failed to set ZCL attribute");
    }
}
