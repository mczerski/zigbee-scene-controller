#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <hal/nrf_usbd.h>
#include <hal/nrf_power.h>


static int usbd_cleanup(void)
{
    NVIC_DisableIRQ(USBD_IRQn);
    NVIC_ClearPendingIRQ(USBD_IRQn);

    nrf_usbd_int_disable(NRF_USBD, UINT32_MAX);
    nrf_usbd_pullup_disable(NRF_USBD);
    nrf_usbd_disable(NRF_USBD);

    nrf_power_int_disable(NRF_POWER, NRF_POWER_INT_USBDETECTED_MASK |
                                     NRF_POWER_INT_USBREMOVED_MASK |
                                     NRF_POWER_INT_USBPWRRDY_MASK);
    nrf_power_event_clear(NRF_POWER, NRF_POWER_EVENT_USBDETECTED);
    nrf_power_event_clear(NRF_POWER, NRF_POWER_EVENT_USBREMOVED);
    nrf_power_event_clear(NRF_POWER, NRF_POWER_EVENT_USBPWRRDY);

    return 0;
}

SYS_INIT(usbd_cleanup, PRE_KERNEL_1, 0);
