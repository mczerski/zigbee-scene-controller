/**
 * @file main.c
 * @brief Battery-powered Zigbee Scene Controller using nRF Connect SDK
 *
 * This implementation creates a Zigbee End Device (ZED) that can control scenes.
 * It's optimized for battery usage with sleep modes between actions.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>

#include <zboss_api.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>
#include <zb_nrf_platform.h>
#include <zigbee/zigbee_zcl_scenes.h>

/* Device endpoint */
#define SCENE_CTRL_ENDPOINT         1

/* Button assignments */
#define BUTTON_SCENE_1              DK_BTN1_MSK  /* Scene 1 activation */
#define BUTTON_SCENE_2              DK_BTN2_MSK  /* Scene 2 activation */
#define BUTTON_SCENE_3              DK_BTN3_MSK  /* Scene 3 activation */
#define BUTTON_SCENE_4              DK_BTN4_MSK  /* Scene 4 activation */

/* LED assignments */
#define STATUS_LED                  DK_LED1

/* Sleep configuration */
#define SLEEP_TIMEOUT_MS            3000  /* Time before sleep after activity */

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/* Forward declarations */
static void button_handler(uint32_t button_state, uint32_t has_changed);
static void device_clusters_init(void);
static void device_clusters_attr_init(void);
static void configure_power_management(void);

/* Zigbee device context */
static zb_ieee_addr_t ieee_addr;
static zb_uint8_t scene_ctrl_endpoint;

/* Timer for sleep management */
static struct k_timer sleep_timer;

/* Current group ID - replace with your actual group ID */
#define DEFAULT_GROUP_ID            0x0001

/* Scene IDs */
#define SCENE_1_ID                  0x01
#define SCENE_2_ID                  0x02
#define SCENE_3_ID                  0x03
#define SCENE_4_ID                  0x04

/**
 * @brief Function to handle device state change events
 */
static void device_state_changed_cb(zb_bufid_t bufid)
{
    zb_zdo_app_signal_hdr_t *signal_header = NULL;
    zb_zdo_app_signal_type_t signal = zb_get_app_signal(bufid, &signal_header);
    
    switch (signal) {
    case ZB_BDB_SIGNAL_DEVICE_REBOOT:
        /* Fall-through */
    case ZB_BDB_SIGNAL_STEERING:
        if (status == RET_OK) {
            /* Device has successfully joined the network */
            dk_set_led_on(STATUS_LED);
            LOG_INF("Network joined");
            
            /* Schedule sleep timer after join */
            k_timer_start(&sleep_timer, K_MSEC(SLEEP_TIMEOUT_MS), K_NO_WAIT);
        } else {
            /* Join failed, retry */
            LOG_ERR("Failed to join network (status: %d)", status);
            zb_bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING);
        }
        break;
        
    case ZB_ZDO_SIGNAL_LEAVE:
        /* Device left the network */
        dk_set_led_off(STATUS_LED);
        LOG_INF("Left network");
        
        /* Restart commissioning */
        zb_bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING);
        break;
        
    default:
        /* Unhandled signal */
        LOG_DBG("Unhandled signal: %d", signal);
        break;
    }

    if (bufid) {
        zb_buf_free(bufid);
    }
}

/**
 * @brief Main application entry point
 */
void main(void)
{
    zb_ret_t zb_err_code;
    
    LOG_INF("Starting Zigbee Scene Controller");
    
    /* Initialize LEDs and buttons */
    if (dk_buttons_init(button_handler) != 0) {
        LOG_ERR("Failed to initialize buttons");
        return;
    }
    
    if (dk_leds_init() != 0) {
        LOG_ERR("Failed to initialize LEDs");
        return;
    }
    
    /* Configure low power mode */
    configure_power_management();
    
    /* Initialize the sleep timer */
    k_timer_init(&sleep_timer, NULL, NULL);
    
    /* Initialize the Zigbee stack */
    ZB_SET_TRACE_LEVEL(TRACE_LEVEL_INFO);
    
    /* ZBOSS signal handler */
    zb_osif_set_user_cb(device_state_changed_cb);
    
    /* Initialize ZBOSS */
    zb_err_code = zboss_start_no_autostart();
    ZB_ERROR_CHECK(zb_err_code);
    
    /* Register device endpoint */
    scene_ctrl_endpoint = SCENE_CTRL_ENDPOINT;
    zb_err_code = zb_ep_register(scene_ctrl_endpoint, ZB_AF_HA_PROFILE_ID, device_clusters_init);
    ZB_ERROR_CHECK(zb_err_code);
    
    /* Initialize device attributes */
    device_clusters_attr_init();
    
    /* Set device address */
    zb_ieee_addr_get(ieee_addr);
    LOG_INF("Device IEEE address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
           ieee_addr[0], ieee_addr[1], ieee_addr[2], ieee_addr[3],
           ieee_addr[4], ieee_addr[5], ieee_addr[6], ieee_addr[7]);
    
    /* Start Zigbee network join process */
    zb_set_network_ed_role(IEEE_CHANNEL_MASK);
    zb_bdb_set_legacy_device_support(1);
    
    /* Start commissioning */
    zb_err_code = zb_bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING);
    ZB_ERROR_CHECK(zb_err_code);
    
    /* Blink LED to indicate startup */
    dk_set_led_on(STATUS_LED);
    k_sleep(K_MSEC(500));
    dk_set_led_off(STATUS_LED);
    
    /* Main loop - process ZBOSS tasks */
    while (1) {
        zboss_main_loop_iteration();
        k_sleep(K_MSEC(10));
    }
}

/**
 * @brief Configure the device clusters for a Scene Controller
 */
static void device_clusters_init(void)
{
    zb_ret_t zb_err_code;
    
    /* Basic cluster */
    zb_zcl_basic_cluster_init(scene_ctrl_endpoint);
    
    /* Power Configuration cluster */
    zb_zcl_power_config_cluster_init(scene_ctrl_endpoint, ZB_ZCL_CLUSTER_SERVER_ROLE);
    
    /* Identify cluster */
    zb_zcl_identify_cluster_init(scene_ctrl_endpoint, ZB_ZCL_CLUSTER_SERVER_ROLE);
    
    /* Scenes cluster - client role for controlling scenes */
    zb_zcl_scenes_cluster_init(scene_ctrl_endpoint, ZB_ZCL_CLUSTER_CLIENT_ROLE);
    
    /* Groups cluster - client role */
    zb_zcl_groups_cluster_init(scene_ctrl_endpoint, ZB_ZCL_CLUSTER_CLIENT_ROLE);
    
    /* Add the device to a group - typically this would be done via commissioning
     * but we hardcode a default group for demonstration purposes
     */
    zb_err_code = zb_zcl_groups_add_group(scene_ctrl_endpoint, DEFAULT_GROUP_ID, "Default");
    if (zb_err_code != RET_OK) {
        LOG_ERR("Failed to add device to group");
    }
}

/**
 * @brief Initialize the cluster attributes
 */
static void device_clusters_attr_init(void)
{
    /* Basic cluster attributes */
    zb_zcl_basic_set_attr_manuf_name(scene_ctrl_endpoint, "Nordic");
    zb_zcl_basic_set_attr_model_id(scene_ctrl_endpoint, "SceneController");
    zb_zcl_basic_set_attr_date_code(scene_ctrl_endpoint, "20250430");
    zb_zcl_basic_set_attr_power_source(scene_ctrl_endpoint, ZB_ZCL_BASIC_POWER_SOURCE_BATTERY);
    
    /* Power configuration attributes */
    uint8_t battery_percentage = 100; // Initial battery level
    zb_zcl_power_config_set_battery_percentage_remaining(scene_ctrl_endpoint, battery_percentage);
}

/**
 * @brief Button event handler for scene activation
 */
static void button_handler(uint32_t button_state, uint32_t has_changed)
{
    zb_ret_t zb_err_code;
    zb_uint8_t scene_id = 0;
    
    /* Reset sleep timer on any button activity */
    k_timer_start(&sleep_timer, K_MSEC(SLEEP_TIMEOUT_MS), K_NO_WAIT);
    
    /* Blink LED to indicate button press */
    dk_set_led_on(STATUS_LED);
    k_sleep(K_MSEC(100));
    dk_set_led_off(STATUS_LED);
    
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
        
        /* Fill the buffer with scene recall command */
        zb_zcl_scenes_recall_req_t *req = ZB_ZCL_START_PACKET(bufid);
        
        req->group_id = DEFAULT_GROUP_ID;
        req->scene_id = scene_id;
        
        /* Send the scene recall command */
        zb_err_code = zb_zcl_scenes_send_recall_req(
            bufid,
            scene_ctrl_endpoint,
            ZB_APS_ADDR_MODE_DST_ADDR_GROUP,
            DEFAULT_GROUP_ID,
            NULL,
            NULL,
            NULL);
            
        if (zb_err_code != RET_OK) {
            LOG_ERR("Failed to send scene recall command, error: %d", zb_err_code);
            zb_buf_free(bufid);
        }
    }
}

/**
 * @brief Configure power management for battery-powered operation
 */
static void configure_power_management(void)
{
    /* Configure sleep mode */
    pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
    
    /* Callback when sleep timer expires */
    k_timer_user_data_set(&sleep_timer, NULL);
    k_timer_expiry_function_set(&sleep_timer, sleep_timeout_handler);
}

/**
 * @brief Handler for sleep timer expiry
 */
static void sleep_timeout_handler(struct k_timer *timer)
{
    LOG_INF("Entering low power mode");
    
    /* Turn off LED */
    dk_set_led_off(STATUS_LED);
    
    /* Enter sleep mode to conserve battery */
    pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
    
    /* The device will remain in low power mode until a button is pressed */
}

/**
 * @brief Update battery level reading periodically
 * This would typically be done with a timer or ADC reading
 */
static void update_battery_level(void)
{
    /* In a real application, read the battery level from ADC
     * This is a placeholder that would be replaced with actual hardware reading
     */
    static uint8_t battery_percentage = 100;
    
    /* Simulate battery drain over time */
    if (battery_percentage > 0) {
        battery_percentage--;
    }
    
    /* Update the battery level attribute */
    zb_zcl_power_config_set_battery_percentage_remaining(scene_ctrl_endpoint, battery_percentage);
    
    LOG_INF("Battery level: %d%%", battery_percentage);
}
