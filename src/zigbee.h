#pragma once
#include <stdint.h>


void configure_zigbee(void);
void send_scene(uint16_t scene_id);
void set_battery_state(uint32_t battery_voltage, float battery_level);
