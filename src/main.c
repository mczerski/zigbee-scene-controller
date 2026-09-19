#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/version.h>
#include <zboss_api.h>

#include "zigbee.h"
#include "battery.h"


LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

int main(void)
{
    LOG_INF("Starting Zigbee R23 Scene Switch %s", FW_VERSION_FULL);
    LOG_INF("Built from commit of %s, Zephyr %s, ZBOSS %d.%d",
            FW_COMMIT_DATE, KERNEL_VERSION_STRING, ZBOSS_MAJOR, ZBOSS_MINOR);

    configure_battery();
    configure_zigbee();

    LOG_INF("Zigbee R23 Scene Switch started");

    k_sleep(K_FOREVER);
    return 0;
}
