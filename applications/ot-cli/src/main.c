#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>

LOG_MODULE_REGISTER(cli_sample, CONFIG_OT_COMMAND_LINE_INTERFACE_LOG_LEVEL);

#define WELLCOME_TEXT \
	"\n\r"\
	"\n\r"\
	"OpenThread Command Line Interface is now running.\n\r" \
	"Use the 'ot' keyword to invoke OpenThread commands e.g. " \
	"'ot thread start.'\n\r" \
	"For the full commands list refer to the OpenThread CLI " \
	"documentation at:\n\r" \
	"https://github.com/openthread/openthread/blob/master/src/cli/README.md\n\r"

#include <openthread/instance.h>
#include <openthread/platform/time.h>
#include <openthread/platform/alarm-milli.h>
#include <openthread/platform/alarm-micro.h>
#include <openthread-system.h>
#include "rtl_wdt.h"
#include "soc.h"
#include "mac_driver.h"

// replace misc.c
extern void WDG_SystemReset(WDTMode_TypeDef wdt_mode, int reset_reason);
void __wrap_otPlatReset(otInstance *aInstance)
{
 	ARG_UNUSED(aInstance);
	WDG_SystemReset(RESET_ALL, 0xff);
}

bool milli_fired = false;
void milli_handler(void)
{
    milli_fired = true;
    otSysEventSignalPending();
}

bool micro_fired = false;
void micro_handler(void)
{
    micro_fired = true;
    otSysEventSignalPending();
}

void __wrap_platformAlarmInit(void)
{
    mac_RegisterBtTimerHandler(MAC_BT_TIMER0, milli_handler);
    mac_RegisterBtTimerHandler(MAC_BT_TIMER1, micro_handler);
}

void __wrap_platformAlarmProcess(otInstance *aInstance)
{
#if OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE
	if (micro_fired) {
		micro_fired = false;
		otPlatAlarmMicroFired(aInstance);
	}
#endif
	if (milli_fired) {
		milli_fired = false;
        otPlatAlarmMilliFired(aInstance);
	}
}

uint32_t __wrap_otPlatAlarmMicroGetNow(void)
{
    return (uint32_t)otPlatTimeGet();
}

void __wrap_otPlatAlarmMicroStartAt(otInstance *aInstance, uint32_t t0, uint32_t dt)
{
    uint64_t now = otPlatTimeGet();
    uint64_t target_us = mac_ConvertT0AndDtTo64BitTime(t0, dt, &now);
    if (target_us > now)
    {
        target_us = target_us % MAX_BT_CLOCK_COUNTER;
        mac_SetBTClkUSInt(MAC_BT_TIMER1, target_us);
    }
    else
    {
        otPlatAlarmMicroFired(aInstance);
    }
}

void __wrap_otPlatAlarmMicroStop(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
}

uint32_t __wrap_otPlatAlarmMilliGetNow(void)
{
    return (uint32_t)(otPlatTimeGet() / 1000);
}

void __wrap_otPlatAlarmMilliStartAt(otInstance *aInstance, uint32_t t0, uint32_t dt)
{
    uint64_t now = otPlatTimeGet();
    uint64_t target_us;
    uint64_t now_ms = now/1000;
    uint64_t target_ms = mac_ConvertT0AndDtTo64BitTime(t0, dt, &now_ms);
    if (target_ms > now_ms)
    {
        target_us = now + (target_ms - now_ms)*1000;
        target_us = target_us % MAX_BT_CLOCK_COUNTER;
        mac_SetBTClkUSInt(MAC_BT_TIMER0, target_us);
    }
    else
    {
        otPlatAlarmMilliFired(aInstance);
    }
}

void __wrap_otPlatAlarmMilliStop(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
}

int main(void)
{
#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_shell_uart), zephyr_cdc_acm_uart)
	int ret;
	const struct device *dev;
	uint32_t dtr = 0U;

	ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return 0;
	}

	dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
	if (dev == NULL) {
		LOG_ERR("Failed to find specific UART device");
		return 0;
	}

	LOG_INF("Waiting for host to be ready to communicate");

	/* Data Terminal Ready - check if host is ready to communicate */
	while (!dtr) {
		ret = uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		if (ret) {
			LOG_ERR("Failed to get Data Terminal Ready line state: %d",
				ret);
			continue;
		}
		k_msleep(100);
	}

	/* Data Carrier Detect Modem - mark connection as established */
	(void)uart_line_ctrl_set(dev, UART_LINE_CTRL_DCD, 1);
	/* Data Set Ready - the NCP SoC is ready to communicate */
	(void)uart_line_ctrl_set(dev, UART_LINE_CTRL_DSR, 1);
#endif

	LOG_INF(WELLCOME_TEXT);

	return 0;
}
