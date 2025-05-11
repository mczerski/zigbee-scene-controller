#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/reboot.h>
#include <dk_buttons_and_leds.h>
#include <ram_pwrdn.h>

#define ZB_HA_DEFINE_DEVICE_SCENE_SELECTOR
#include <zboss_api.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>
#include <zb_nrf_platform.h>
#include "my_device.h"

#define BUTTON_MSK                 DK_BTN1_MSK  /* Scene 1 activation */
#define MY_DEVICE_ENDPOINT         1
#define BATTERY_INTERVAL           60000

/* Do not erase NVRAM to save the network parameters after device reboot or
 * power-off. NOTE: If this option is set to ZB_TRUE then do full device erase
 * for all network devices before running other samples.
 */
#define ERASE_PERSISTENT_CONFIG    ZB_FALSE
/* LED indicating that light switch successfully joind Zigbee network. */
#define ZIGBEE_NETWORK_STATE_LED   DK_LED1
/* LED used for device identification. */
#define IDENTIFY_LED               DK_LED2

/* Button to start Factory Reset */
#define FACTORY_RESET_BUTTON       DK_BTN1_MSK

#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE to compile light switch (End Device) source code.
#endif

#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || \
    !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified"
#endif

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define DT_SPEC_AND_COMMA(node_id, prop, idx) ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {
    DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels, DT_SPEC_AND_COMMA)
};

struct buttons_context {
    int state;
    struct k_timer timer;
};

static struct buttons_context buttons_ctx;

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
    ZB_ZCL_SCENES_SEND_RECALL_SCENE_REQ(
        bufid,
        addr,
        ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        1,
        MY_DEVICE_ENDPOINT,
        ZB_AF_HA_PROFILE_ID,
        ZB_ZCL_DISABLE_DEFAULT_RESPONSE,
        on_off_callback,
        0, // group id
        cmd_id
    )
    LOG_INF("Sent scene command: %d", cmd_id);
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
            k_timer_start(&buttons_ctx.timer, K_MSEC(500), K_NO_WAIT);
        }
        else if (!was_factory_reset_done()) {
            LOG_INF("Button released");
            k_timer_stop(&buttons_ctx.timer);
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
    if (has_changed & DK_BTN4_MSK) {
        NRF_POWER->GPREGRET = 0x57;
        sys_reboot(SYS_REBOOT_COLD);
    }
}

static void button_timer_handler(struct k_timer *timer)
{
    if (dk_get_buttons() & BUTTON_MSK) {
        buttons_ctx.state += 1;
        k_timer_start(&buttons_ctx.timer, K_MSEC(1500), K_NO_WAIT);
    }
}

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
    k_timer_init(&buttons_ctx.timer, button_timer_handler, NULL);
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
    if (bufid) {
        LOG_INF("Identify started");
        /* Schedule a self-scheduling function that will toggle the LED. */
        ZB_SCHEDULE_APP_CALLBACK(toggle_identify_led, bufid);
    } else {
        LOG_INF("Identify stopped");
        /* Cancel the toggling function alarm and turn off LED. */
        ZB_SCHEDULE_APP_ALARM_CANCEL(toggle_identify_led, ZB_ALARM_ANY_PARAM);
        dk_set_led_off(IDENTIFY_LED);
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
        /* start battery measurement timer */
        ZB_SCHEDULE_APP_CALLBACK(battery_alarm_handler, ZB_ALARM_ANY_PARAM);
        /* Call default signal handler. */
        //zb_zdo_pim_set_long_poll_interval(100000);
        ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));
    case ZB_ZDO_SIGNAL_LEAVE:
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
    configure_gpio();
    configure_timers();
    configure_adc();
    register_factory_reset_button(FACTORY_RESET_BUTTON);

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
