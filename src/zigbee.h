#pragma once
#include <stdint.h>


void configure_zigbee(void);
void send_scene(uint16_t scene_id);
void set_battery_state(int32_t battery_voltage_mv, int32_t battery_level_dp);
