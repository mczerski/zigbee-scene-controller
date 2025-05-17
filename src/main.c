#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "zigbee.h"
#include "battery.h"


LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

int main(void)
{
    LOG_INF("Starting Zigbee R23 Scene Switch");

    configure_battery();
    configure_zigbee();

    LOG_INF("Zigbee R23 Scene Switch started");

    k_sleep(K_FOREVER);
    return 0;
}
