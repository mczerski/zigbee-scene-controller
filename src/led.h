#pragma once
#include <stdint.h>
#include <stdbool.h>


void off_state_led(uint8_t);
void blink_state_led(uint32_t on_ms, uint32_t off_ms, uint8_t count);
void set_state_led(bool state);
