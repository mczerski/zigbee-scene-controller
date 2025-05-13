#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/led.h>
#include <zephyr/input/input.h>
#include <ram_pwrdn.h>

#define ZB_HA_DEFINE_DEVICE_SCENE_SELECTOR
#include <zboss_api.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>
#include <zb_nrf_platform.h>
#include "my_device.h"

#define MY_DEVICE_ENDPOINT         1
#define BATTERY_INTERVAL           60000

/* Do not erase NVRAM to save the network parameters after device reboot or
 * power-off. NOTE: If this option is set to ZB_TRUE then do full device erase
 * for all network devices before running other samples.
 */
#define ERASE_PERSISTENT_CONFIG    ZB_FALSE

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define DT_SPEC_AND_COMMA(node_id, prop, idx) ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {
    DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels, DT_SPEC_AND_COMMA)
};

const struct device * led_dev = DEVICE_DT_GET(DT_PARENT(DT_ALIAS(led1)));
uint32_t led_idx = DT_NODE_CHILD_IDX(DT_ALIAS(led1));

static void off_state_led(zb_uint8_t param)
{
    led_off(led_dev, led_idx);
}

static void blink_state_led(uint32_t ms)
{
    led_on(led_dev, led_idx);
    ZB_SCHEDULE_APP_ALARM(off_state_led, ZB_ALARM_ANY_PARAM, ZB_MILLISECONDS_TO_BEACON_INTERVAL(ms));
}

/* Basic cluster attributes data */
// TODO: use deicated macros to prepare strings
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
ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(identify_attr_list, &g_attr_identify_identify_time);

/* Power configuration cluster attributes data */
zb_uint8_t g_attr_battery_voltage = ZB_ZCL_POWER_CONFIG_BATTERY_VOLTAGE_INVALID;
zb_uint8_t g_attr_battery_size = ZB_ZCL_POWER_CONFIG_BATTERY_SIZE_BUILT_IN;
zb_uint8_t g_attr_battery_quantity = 1;
zb_uint8_t g_attr_battery_rated_voltage = 3700 / 100; // 100mV unit
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

static void scene_callback(zb_bufid_t buffer)
{
    zb_uint8_t status = zb_buf_get_status(buffer);

    LOG_INF("scene_callback %d", buffer);

    LOG_INF("buffer %d", buffer);
    LOG_INF("status %d", status);

    /* Free the buffer */
    zb_buf_free(buffer);
}

static void send_scene(zb_bufid_t bufid, zb_uint16_t scene_id)
{
    uint16_t addr = 0x0000;
    ZB_ZCL_SCENES_SEND_RECALL_SCENE_REQ(
        bufid,
        addr,
        ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        1,
        MY_DEVICE_ENDPOINT,
        ZB_AF_HA_PROFILE_ID,
        ZB_ZCL_DISABLE_DEFAULT_RESPONSE,
        scene_callback,
        0, // group id
        scene_id
    )
    LOG_INF("Sent scene command: 0x%x", scene_id);
    blink_state_led(200);
}

static void button_handler(struct input_event *evt, void *user_data)
{
    LOG_INF("Button event. type=%d, code=0x%x, value=%d", evt->type, evt->code, evt->value);

    if (evt->type != INPUT_EV_KEY) {
        return;
    }
    /* Inform default signal handler about user input at the device. */
    user_input_indicate();

    zb_uint16_t scene_type = evt->code >> 4;

    if (
        (scene_type == 2 && evt->value == 1) || // long press activation
        (scene_type == 3 && evt->value == 0) || // single press
        (scene_type == 4 && evt->value == 0) // double press
    ) {
        zb_ret_t zb_err_code = zb_buf_get_out_delayed_ext(send_scene, evt->code, 0);
        if (zb_err_code) {
            LOG_WRN("Buffer is full");
        }
    }
}

INPUT_CALLBACK_DEFINE(NULL, button_handler, NULL);

static void battery_alarm_handler(zb_bufid_t bufid)
{
    ZB_SCHEDULE_APP_ALARM(battery_alarm_handler, ZB_ALARM_ANY_PARAM, ZB_MILLISECONDS_TO_BEACON_INTERVAL(BATTERY_INTERVAL));

    uint16_t buf;
    struct adc_sequence sequence = {
        .buffer = &buf,
        /* buffer size in bytes, not number of samples */
        .buffer_size = sizeof(buf),
    };
    adc_sequence_init_dt(&adc_channels[0], &sequence);
    int err = adc_read_dt(&adc_channels[0], &sequence);
    if (err < 0) {
        LOG_ERR("Could not read (%d)", err);
    }
    int32_t val_mv = (int32_t)buf;
    err = adc_raw_to_millivolts_dt(&adc_channels[0], &val_mv);
    /* conversion to mV may not be supported, skip if not */
    if (err < 0) {
        LOG_ERR("value in mV not available");
    }
    else {
        val_mv *= 5;
        LOG_ERR("battery timer read value: %d", val_mv);
    }

    zb_uint8_t battery_attribute = val_mv / 100;
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
    battery_attribute = 200 * val_mv / 5200;
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

static void configure_adc(void)
{
    int err;
    /* Configure channels individually prior to sampling. */
    for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
        if (!adc_is_ready_dt(&adc_channels[i])) {
            LOG_ERR("ADC controller device %s not ready\n", adc_channels[i].dev->name);
            return;
        }

        err = adc_channel_setup_dt(&adc_channels[i]);
        if (err < 0) {
            LOG_ERR("Could not setup channel #%d (%d)\n", i, err);
            return;
        }
    }
}

static void toggle_identify_led(zb_uint8_t param)
{
    static int blink_status;

    led_set_brightness(led_dev, led_idx, (++blink_status) % 2);
    ZB_SCHEDULE_APP_ALARM(toggle_identify_led, param, ZB_MILLISECONDS_TO_BEACON_INTERVAL(100));
}

/**@brief Function to handle identify notification events on the first endpoint.
 *
 * @param  bufid  Unused parameter, required by ZBOSS scheduler API.
 */
static void identify_cb(zb_bufid_t bufid)
{
    if (bufid) {
        LOG_INF("Identify started");
        /* Schedule a self-scheduling function that will toggle the LED. */
        ZB_SCHEDULE_APP_CALLBACK(toggle_identify_led, ZB_ALARM_ANY_PARAM);
    } else {
        LOG_INF("Identify stopped");
        /* Cancel the toggling function alarm and turn off LED. */
        ZB_SCHEDULE_APP_ALARM_CANCEL(toggle_identify_led, ZB_ALARM_ANY_PARAM);
        led_off(led_dev, led_idx);
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

    switch (sig) {
    case ZB_BDB_SIGNAL_DEVICE_REBOOT:
    /* fall-through */
    case ZB_BDB_SIGNAL_STEERING:
        if (status == RET_OK) {
            blink_state_led(200);
        }
        else {
            off_state_led(0);
        }
        /* start battery measurement timer */
        ZB_SCHEDULE_APP_CALLBACK(battery_alarm_handler, ZB_ALARM_ANY_PARAM);
        //zb_zdo_pim_set_long_poll_interval(100000);
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

    if (bufid) {
        zb_buf_free(bufid);
    }
}

int main(void)
{
    LOG_INF("Starting Zigbee R23 Light Switch example");

    /* Initialize. */
    configure_adc();

    zigbee_erase_persistent_storage(ERASE_PERSISTENT_CONFIG);
    //zb_set_ed_timeout(ED_AGING_TIMEOUT_64MIN);
    //zb_set_keepalive_timeout(ZB_SECONDS_TO_BEACON_INTERVAL(100));
    zigbee_configure_sleepy_behavior(true);
// TODO: check if makes difference, if so fix
//    power_down_unused_ram();

    /* Register device context (endpoints). */
    ZB_AF_REGISTER_DEVICE_CTX(&device_ctx);

    /* Register handlers to identify notifications */
    ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(MY_DEVICE_ENDPOINT, identify_cb);

    /* Start Zigbee default thread. */
    zigbee_enable();

    LOG_INF("Zigbee R23 Light Switch example started");

    k_sleep(K_FOREVER);
    return 0;
}
