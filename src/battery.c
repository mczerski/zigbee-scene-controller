#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/dt-bindings/adc/nrf-saadc-v3.h>
#include <zboss_api.h>
#include "zigbee.h"


#define BATTERY_INTERVAL           30*60000

/* VDDHDIV5 measures the battery through the internal 1/5 divider, plain VDD
 * measures the regulated rail, so each has its own empty/full range */
#define VDDHDIV5_RATIO             5
#define VDDH_FULL_MV               4200
#define VDDH_EMPTY_MV              3000
#define VDD_FULL_MV                3200
#define VDD_EMPTY_MV               2000
/* charge level is reported in decipercents */
#define LEVEL_FULL_DP              1000

LOG_MODULE_REGISTER(battery, LOG_LEVEL_INF);

#define DT_SPEC_AND_COMMA(node_id, prop, idx) ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {
    DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels, DT_SPEC_AND_COMMA)
};

void battery_alarm_handler(uint8_t)
{
    ZB_SCHEDULE_APP_ALARM(battery_alarm_handler, ZB_ALARM_ANY_PARAM, ZB_MILLISECONDS_TO_BEACON_INTERVAL(BATTERY_INTERVAL));

    int16_t buf;
    struct adc_sequence sequence = {
        .buffer = &buf,
        /* buffer size in bytes, not number of samples */
        .buffer_size = sizeof(buf),
    };
    adc_sequence_init_dt(&adc_channels[0], &sequence);
    int err = adc_read_dt(&adc_channels[0], &sequence);
    if (err < 0) {
        LOG_ERR("Could not read (%d)", err);
        return;
    }
    int32_t val_mv = buf;
    adc_raw_to_millivolts_dt(&adc_channels[0], &val_mv);
    if (adc_channels[0].channel_cfg.input_positive == NRF_SAADC_VDDHDIV5) {
        val_mv *= VDDHDIV5_RATIO;
    }
    LOG_INF("battery read value: %d", val_mv);

    int32_t max_mv = adc_channels[0].channel_cfg.input_positive == NRF_SAADC_VDDHDIV5 ? VDDH_FULL_MV : VDD_FULL_MV;
    int32_t min_mv = adc_channels[0].channel_cfg.input_positive == NRF_SAADC_VDDHDIV5 ? VDDH_EMPTY_MV : VDD_EMPTY_MV;
    // decy-percents
    int32_t level_dp = CLAMP(LEVEL_FULL_DP * (val_mv - min_mv) / (max_mv - min_mv), 0, LEVEL_FULL_DP);
    set_battery_state(val_mv, level_dp);
}

void configure_battery(void)
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
