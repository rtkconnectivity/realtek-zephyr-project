#include <stdio.h>
#include <zephyr/shell/shell.h>
#include "trace.h"
#ifdef CONFIG_GPIO
#include <zephyr/drivers/gpio.h>
#include <zephyr/dt-bindings/gpio/realtek-bee-gpio.h>
#endif
#ifdef CONFIG_SERIAL
#include <zephyr/drivers/uart.h>
#endif
#ifdef CONFIG_PWM
#include <zephyr/drivers/pwm.h>
#endif
#ifdef CONFIG_COUNTER
#include <zephyr/drivers/counter.h>
#endif
#ifdef CONFIG_SPI
#include <zephyr/drivers/spi.h>
#endif
#ifdef CONFIG_RTC
#include <zephyr/drivers/rtc.h>
#endif
#ifdef CONFIG_I2C
#include <zephyr/drivers/i2c.h>
#endif
#ifdef CONFIG_ADC
#include <zephyr/drivers/adc.h>
#endif
#ifdef CONFIG_SENSOR
#include <zephyr/drivers/sensor.h>
#if defined(CONFIG_QDEC_BEE)
#include <zephyr/drivers/sensor/qdec_bee.h>
#endif
#endif
#ifdef CONFIG_SDMMC_STACK
#include <zephyr/sd/sdmmc.h>
#endif
#ifdef CONFIG_CAN
#include <zephyr/drivers/can.h>
#endif
#if defined(CONFIG_SOC_SERIES_RTL87X2G)
#include <pm.h>
#include "power_manager_unit_platform.h"
#elif defined(CONFIG_SOC_SERIES_RTL8752H)
#include <dlps.h>

extern void (*platform_pm_register_callback_func_with_priority)(void *cb_func,
                                                                PlatformPMStage pf_pm_stage,
                                                                int8_t priority);
#endif

struct k_sem app_sem;

static PMCheckResult dlps_check_flag = PM_CHECK_FAIL;

static PMCheckResult app_check(void) { return dlps_check_flag; }

static void app_store(void) { DBG_DIRECT("[%s] line%d", __func__, __LINE__); }

static void app_restore(void) {
    DBG_DIRECT("[%s] line%d", __func__, __LINE__);
    k_sem_give(&app_sem);
}

int main(void) {
    DBG_DIRECT("[%s] line%d", __func__, __LINE__);
    printf("[%lld] Hello World! %s\n", k_uptime_get(), CONFIG_BOARD_TARGET);

    k_sem_init(&app_sem, 0, 1);

    dlps_check_flag = PM_CHECK_FAIL;
    /* register dlps_check_flag */
    DBG_DIRECT("[%s] line%d", __func__, __LINE__);
    platform_pm_register_callback_func_with_priority((void *)app_check, PLATFORM_PM_CHECK, 1);
    DBG_DIRECT("[%s] line%d", __func__, __LINE__);

    platform_pm_register_callback_func_with_priority((void *)app_store, PLATFORM_PM_STORE, 1);
    platform_pm_register_callback_func_with_priority((void *)app_restore, PLATFORM_PM_RESTORE, 1);
    DBG_DIRECT("[%s] line%d", __func__, __LINE__);
    return 0;
}

static int shell_pm_test_uart(const struct shell *sh, size_t argc, char **argv) {
#ifdef CONFIG_SERIAL
    /* enter dlps */
    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    printf("[%lld] before enter dlps\n", k_uptime_get());
    printf("[%lld] type on shell to wakeup\n", k_uptime_get());
    dlps_check_flag = PM_CHECK_PASS;
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_FOREVER);

    dlps_check_flag = PM_CHECK_FAIL;

    /* rx 1 byte to wakeup */
    DBG_DIRECT("[%s] wakeup", __func__);
    printf("[%lld] after enter dlps\n", k_uptime_get());
#endif

    return 0;
}

#ifdef CONFIG_COUNTER
static void top_handler(const struct device *dev, void *user_data) {
    DBG_DIRECT("top_handler");
    uint64_t *pre_sys_time_ms = (uint64_t *)user_data;
    uint64_t current_sys_time_ms = k_uptime_get();

    /* print current time */
    printf("[%lld] trigger handler after %lldms\n", current_sys_time_ms,
           current_sys_time_ms - *pre_sys_time_ms);

    k_sem_give(&app_sem);
}
#endif

static int shell_pm_test_counter(const struct shell *sh, size_t argc, char **argv) {
#ifdef CONFIG_COUNTER
    const struct device *test_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(test_counter_timer));
    struct counter_top_cfg top_cfg;
    uint64_t current_sys_time_ms;
    uint64_t timeout_ms = atoi(argv[1]);

    counter_start(test_dev);

    top_cfg.callback = top_handler;
    top_cfg.flags = 0;
    top_cfg.ticks = counter_us_to_ticks(test_dev, timeout_ms * 1000);
    current_sys_time_ms = k_uptime_get();
    top_cfg.user_data = &current_sys_time_ms;
    printf("[%lld] wait %lldms to trigger handler\n", current_sys_time_ms, timeout_ms);
    counter_set_top_value(test_dev, &top_cfg);

    /* enter dlps */
    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    dlps_check_flag = PM_CHECK_PASS;
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_FOREVER);

    dlps_check_flag = PM_CHECK_FAIL;
    DBG_DIRECT("[%s] wakeup", __func__);

    counter_stop(test_dev);
#endif

    return 0;
}

#if CONFIG_PM_DEVICE
#ifdef CONFIG_GPIO
#define DEV_OUT DT_GPIO_CTLR(DT_INST(0, test_gpio_basic_api), out_gpios)
#define DEV_IN DT_GPIO_CTLR(DT_INST(0, test_gpio_basic_api), in_gpios)
#define DEV DEV_OUT
#define PIN_OUT DT_GPIO_PIN(DT_INST(0, test_gpio_basic_api), out_gpios)
#define PIN_OUT_FLAGS DT_GPIO_FLAGS(DT_INST(0, test_gpio_basic_api), out_gpios)
#define PIN_IN DT_GPIO_PIN(DT_INST(0, test_gpio_basic_api), in_gpios)
#define PIN_IN_FLAGS DT_GPIO_FLAGS(DT_INST(0, test_gpio_basic_api), in_gpios)
struct gpio_callback gpio_cb;

static void callback(const struct device *dev_in, struct gpio_callback *gpio_cb, uint32_t pins) {
    static uint8_t cnt;
    DBG_DIRECT("[%d] enter gpio callback cnt%d\n", k_uptime_get(), cnt);
    dlps_check_flag = PM_CHECK_FAIL;
    printf("[%lld] enter gpio callback cnt%d\n", k_uptime_get(), cnt);
    ++cnt;
}
#endif

static int shell_pm_test_gpio(const struct shell *sh, size_t argc, char **argv) {
#ifdef CONFIG_GPIO
    const struct device *const dev_in = DEVICE_DT_GET_OR_NULL(DEV_IN);
    const struct device *const dev_out = DEVICE_DT_GET_OR_NULL(DEV_OUT);

    /* 1. set PIN_OUT to logical initial state inactive */
    gpio_pin_configure(dev_out, PIN_OUT, GPIO_OUTPUT_LOW | PIN_OUT_FLAGS);

    /* 2. configure PIN_IN callback and trigger condition */
    gpio_pin_configure(dev_in, PIN_IN,
                       (GPIO_INPUT | GPIO_PULL_UP | PIN_IN_FLAGS | BEE_GPIO_INPUT_PM_WAKEUP));
    gpio_init_callback(&gpio_cb, callback, BIT(PIN_IN));
    gpio_add_callback(dev_in, &gpio_cb);
    gpio_pin_interrupt_configure(dev_in, PIN_IN, GPIO_INT_EDGE_FALLING);

    printf("[%lld] connect input pin to output pin\n", k_uptime_get());

    /* enter dlps */
    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    printf("[%lld] before enter dlps\n", k_uptime_get());
    printf("[%lld] disconnect input pin to output pin to wakeup\n", k_uptime_get());
    dlps_check_flag = PM_CHECK_PASS;
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_FOREVER);

    dlps_check_flag = PM_CHECK_FAIL;

    /* rising edge to wakeup */
    DBG_DIRECT("[%s] wakeup", __func__);
    printf("[%lld] after enter dlps\n", k_uptime_get());

    k_busy_wait(100000);

    gpio_pin_interrupt_configure(dev_in, PIN_IN, GPIO_INT_EDGE_RISING);

    /* enter dlps */
    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    printf("[%lld] before enter dlps\n", k_uptime_get());
    printf("[%lld] connect input pin to output pin to wakeup\n", k_uptime_get());
    dlps_check_flag = PM_CHECK_PASS;
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_FOREVER);

    dlps_check_flag = PM_CHECK_FAIL;

    /* falling edge to wakeup */
    DBG_DIRECT("[%s] wakeup", __func__);
    printf("[%lld] after enter dlps\n", k_uptime_get());

    k_busy_wait(100000);
#if defined(CONFIG_SOC_SERIES_RTL8752H)
    /* both edge */
    gpio_pin_interrupt_configure(dev_in, PIN_IN, GPIO_INT_EDGE_BOTH);

    /* enter dlps */
    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    printf("[%lld] before enter dlps\n", k_uptime_get());
    printf("[%lld] disconnect input pin to output pin to wakeup\n", k_uptime_get());
    dlps_check_flag = PM_CHECK_PASS;
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_FOREVER);

    dlps_check_flag = PM_CHECK_FAIL;

    /* rising edge to wakeup */
    DBG_DIRECT("[%s] wakeup", __func__);
    printf("[%lld] after enter dlps\n", k_uptime_get());

    k_busy_wait(100000);

    /* enter dlps */
    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    printf("[%lld] before enter dlps\n", k_uptime_get());
    printf("[%lld] connect input pin to output pin to wakeup\n", k_uptime_get());
    dlps_check_flag = PM_CHECK_PASS;
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_FOREVER);

    dlps_check_flag = PM_CHECK_FAIL;

    /* falling edge to wakeup */
    DBG_DIRECT("[%s] wakeup", __func__);
    printf("[%lld] after enter dlps\n", k_uptime_get());

    k_busy_wait(100000);

#endif

    gpio_remove_callback(dev_in, &gpio_cb);

    gpio_pin_interrupt_configure(dev_in, PIN_IN, GPIO_INT_DISABLE);
    gpio_pin_configure(dev_in, PIN_IN,
                       (GPIO_INPUT | GPIO_PULL_UP | PIN_IN_FLAGS & (~BEE_GPIO_INPUT_PM_WAKEUP)));

#endif
    return 0;
}

static int shell_pm_test_pwm(const struct shell *sh, size_t argc, char **argv) {
#ifdef CONFIG_PWM
    const struct device *test_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(test_pwm));
    ;
    uint32_t period, pulse;

    printf("[%lld] connect pwm pin to LA to watch the waveform\n", k_uptime_get());

    period = 50000;
    pulse = 10000;
    printf("[%lld] [PWM]: %s, [period]: %d, [pulse]: %d\n", k_uptime_get(), test_dev->name, period,
           pulse);
    pwm_set_cycles(test_dev, 0, period, pulse, 0);
    k_busy_wait(500000);

    /* enter dlps */
    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    printf("[%lld] before enter dlps\n", k_uptime_get());
    dlps_check_flag = PM_CHECK_PASS;
    /* delay 500 ms to wakeup */
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_MSEC(500));
    dlps_check_flag = PM_CHECK_FAIL;

    DBG_DIRECT("[%s] wakeup", __func__);
    printf("[%lld] after enter dlps\n", k_uptime_get());
    k_busy_wait(500000);

    period = 0;
    pulse = 0;
    printf("[%lld] [PWM]: %s, [period]: %d, [pulse]: %d\n", k_uptime_get(), test_dev->name, period,
           pulse);
    pwm_set_cycles(test_dev, 0, period, pulse, 0);
    k_busy_wait(500000);

    k_sleep(K_MSEC(10));

    period = 50000;
    pulse = 40000;
    printf("[%lld] [PWM]: %s, [period]: %d, [pulse]: %d\n", k_uptime_get(), test_dev->name, period,
           pulse);
    pwm_set_cycles(test_dev, 0, period, pulse, 0);
    k_busy_wait(500000);

    /* enter dlps */
    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    printf("[%lld] before enter dlps\n", k_uptime_get());
    dlps_check_flag = PM_CHECK_PASS;
    /* delay 500 ms to wakeup */
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_MSEC(500));
    dlps_check_flag = PM_CHECK_FAIL;

    DBG_DIRECT("[%s] wakeup", __func__);
    printf("[%lld] after enter dlps\n", k_uptime_get());
    k_busy_wait(500000);

    period = 0;
    pulse = 0;
    printf("[%lld] [PWM]: %s, [period]: %d, [pulse]: %d\n", k_uptime_get(), test_dev->name, period,
           pulse);
    pwm_set_cycles(test_dev, 0, period, pulse, 0);

#endif
    return 0;
}

#ifdef CONFIG_SPI

#define MODE_LOOP 0
#define FRAME_SIZE (8)
#define SPI_OP(frame_size)                                                                         \
    SPI_OP_MODE_MASTER | SPI_MODE_CPOL | SPI_MODE_CPHA | SPI_WORD_SET(frame_size) | SPI_LINES_SINGLE

#define SPI_DEV DT_COMPAT_GET_ANY_STATUS_OKAY(test_spi_loopback)
#define BUF_SIZE 18

static const char tx_data[BUF_SIZE] = "0123456789abcdef-\0";
static __aligned(32) char buffer_tx[BUF_SIZE];
static __aligned(32) char buffer_rx[BUF_SIZE];

static int spi_complete_loop(struct spi_dt_spec *spec) {
    memcpy(buffer_tx, tx_data, sizeof(tx_data));
    memset(buffer_rx, 0, sizeof(buffer_rx));

    const struct spi_buf tx_bufs[] = {
        {
            .buf = buffer_tx,
            .len = BUF_SIZE,
        },
    };
    const struct spi_buf rx_bufs[] = {
        {
            .buf = buffer_rx,
            .len = BUF_SIZE,
        },
    };
    const struct spi_buf_set tx = {.buffers = tx_bufs, .count = ARRAY_SIZE(tx_bufs)};
    const struct spi_buf_set rx = {.buffers = rx_bufs, .count = ARRAY_SIZE(rx_bufs)};

    int ret;

    printf("[%lld] Start complete loop\n", k_uptime_get());

    ret = spi_transceive_dt(spec, &tx, &rx);

    if (memcmp(buffer_tx, buffer_rx, BUF_SIZE)) {
        printf("[%lld] Buffer contents are different\n", k_uptime_get());
        return -1;
    }

    printf("[%lld] Buffer contents are same\n", k_uptime_get());
    return 0;
}
#endif

static int shell_pm_test_spi(const struct shell *sh, size_t argc, char **argv) {
#ifdef CONFIG_SPI
    static struct spi_dt_spec spi_spec = SPI_DT_SPEC_GET(SPI_DEV, SPI_OP(FRAME_SIZE), 0);

    printf("[%lld] connect MOSI pin to the MISO of the SPI\n", k_uptime_get());

    if (spi_complete_loop(&spi_spec) < 0) {
        printf("[%lld] loopback test fali\n", k_uptime_get());
        return 0;
    }

    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    printf("[%lld] before enter dlps\n", k_uptime_get());
    printf("[%lld] type on shell to wakeup\n", k_uptime_get());
    dlps_check_flag = PM_CHECK_PASS;
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_FOREVER);

    dlps_check_flag = PM_CHECK_FAIL;

    /* rx 1 byte to wakeup */
    DBG_DIRECT("[%s] wakeup", __func__);
    printf("[%lld] after enter dlps\n", k_uptime_get());

    if (spi_complete_loop(&spi_spec) < 0) {
        printf("[%lld] loopback test fali\n", k_uptime_get());
        return 0;
    }

#endif
    return 0;
}

#ifdef CONFIG_RTC
static const struct rtc_time test_rtc_time_set = {
    .tm_sec = 50,
    .tm_min = 29,
    .tm_hour = 13,
    .tm_mday = 1,
    .tm_mon = 0,
    .tm_year = 121,
    .tm_wday = 5,
    .tm_yday = 1,
    .tm_isdst = -1,
    .tm_nsec = 0,
};

static const struct rtc_time test_alarm_time_set = {
    .tm_sec = 52,
    .tm_min = 29,
    .tm_hour = 13,
    .tm_mday = 1,
    .tm_mon = 0,
    .tm_year = 121,
    .tm_wday = 5,
    .tm_yday = 1,
    .tm_isdst = -1,
    .tm_nsec = 0,
};

static void test_rtc_alarm_callback_handler(const struct device *dev, uint16_t id,
                                            void *user_data) {
    uint64_t *pre_sys_time_ms = (uint64_t *)user_data;
    uint64_t current_sys_time_ms = k_uptime_get();
    printf("pre_sys_time_ms %lldms\n", *pre_sys_time_ms);

    /* print current time */
    printf("[%lld] trigger handler after %dms\n", current_sys_time_ms,
           current_sys_time_ms - *pre_sys_time_ms);

    k_sem_give(&app_sem);
}
#endif

static int shell_pm_test_rtc(const struct shell *sh, size_t argc, char **argv) {
#ifdef CONFIG_RTC
    static const struct device *test_rtc = DEVICE_DT_GET_OR_NULL(DT_ALIAS(test_rtc));
    uint64_t current_sys_time_ms;
    rtc_alarm_set_callback(test_rtc, 0, NULL, NULL);
    rtc_set_time(test_rtc, &test_rtc_time_set);

    current_sys_time_ms = k_uptime_get();
    rtc_alarm_set_callback(test_rtc, 0, test_rtc_alarm_callback_handler, &current_sys_time_ms);

    rtc_alarm_set_time(test_rtc, 0, 0x1ff, &test_alarm_time_set);

    printf("[%lld] wait %dms to trigger handler\n", current_sys_time_ms, 2000);

    /* enter dlps */
    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    dlps_check_flag = PM_CHECK_PASS;
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_FOREVER);

    dlps_check_flag = PM_CHECK_FAIL;
    printf("[%s] wakeup\n", __func__);
    DBG_DIRECT("[%s] wakeup", __func__);
#endif
    return 0;
}

static int shell_pm_test_qdec(const struct shell *sh, size_t argc, char **argv) {
#if defined(CONFIG_SENSOR) && defined(CONFIG_QDEC_BEE)
    static const struct gpio_dt_spec phase_a = GPIO_DT_SPEC_GET(DT_ALIAS(test_qenca), gpios);
    static const struct gpio_dt_spec phase_b = GPIO_DT_SPEC_GET(DT_ALIAS(test_qencb), gpios);
    static bool toggle_a;
    struct sensor_value val;
    const struct device *const dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(test_qdec));

    gpio_pin_configure_dt(&phase_a, GPIO_OUTPUT);
    gpio_pin_configure_dt(&phase_b, GPIO_OUTPUT);

    k_busy_wait(100000);

    for (int i = 0; i < 12; i++) {
        toggle_a = !toggle_a;
        if (toggle_a) {
            gpio_pin_toggle_dt(&phase_a);
        } else {
            gpio_pin_toggle_dt(&phase_b);
        }
        k_busy_wait(100000);
        sensor_sample_fetch(dev);
        sensor_channel_get(dev, SENSOR_ATTR_QDEC_X_ROTATION, &val);
        printf("Position[%d] = %d degrees\n", i, val.val1);
    }

    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    printf("[%lld] before enter dlps\n", k_uptime_get());
    printf("[%lld] type on shell to wakeup\n", k_uptime_get());
    dlps_check_flag = PM_CHECK_PASS;
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_FOREVER);

    dlps_check_flag = PM_CHECK_FAIL;

    /* rx 1 byte to wakeup */
    DBG_DIRECT("[%s] wakeup", __func__);
    printf("[%lld] after enter dlps\n", k_uptime_get());

    for (int i = 0; i < 12; i++) {
        toggle_a = !toggle_a;
        if (!toggle_a) {
            gpio_pin_toggle_dt(&phase_a);
        } else {
            gpio_pin_toggle_dt(&phase_b);
        }
        k_busy_wait(100000);
        sensor_sample_fetch(dev);
        sensor_channel_get(dev, SENSOR_ATTR_QDEC_X_ROTATION, &val);
        printf("Position[%d] = %d degrees\n", i, val.val1);
    }

    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    printf("[%lld] before enter dlps\n", k_uptime_get());
    printf("[%lld] type on shell to wakeup\n", k_uptime_get());
    dlps_check_flag = PM_CHECK_PASS;
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_FOREVER);

    dlps_check_flag = PM_CHECK_FAIL;

    /* rx 1 byte to wakeup */
    DBG_DIRECT("[%s] wakeup", __func__);
    printf("[%lld] after enter dlps\n", k_uptime_get());

    for (int i = 0; i < 12; i++) {
        toggle_a = !toggle_a;
        if (!toggle_a) {
            gpio_pin_toggle_dt(&phase_a);
        } else {
            gpio_pin_toggle_dt(&phase_b);
        }
        k_busy_wait(100000);
        sensor_sample_fetch(dev);
        sensor_channel_get(dev, SENSOR_ATTR_QDEC_X_ROTATION, &val);
        printf("Position[%d] = %d degrees\n", i, val.val1);
    }

    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    printf("[%lld] before enter dlps\n", k_uptime_get());
    printf("[%lld] type on shell to wakeup\n", k_uptime_get());
    dlps_check_flag = PM_CHECK_PASS;
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_FOREVER);

    dlps_check_flag = PM_CHECK_FAIL;

    /* rx 1 byte to wakeup */
    DBG_DIRECT("[%s] wakeup", __func__);
    printf("[%lld] after enter dlps\n", k_uptime_get());

    for (int i = 0; i < 12; i++) {
        toggle_a = !toggle_a;
        if (toggle_a) {
            gpio_pin_toggle_dt(&phase_a);
        } else {
            gpio_pin_toggle_dt(&phase_b);
        }
        k_busy_wait(100000);
        sensor_sample_fetch(dev);
        sensor_channel_get(dev, SENSOR_ATTR_QDEC_X_ROTATION, &val);
        printf("Position[%d] = %d degrees\n", i, val.val1);
    }
#endif
    return 0;
}

static int shell_pm_test_aon_qdec(const struct shell *sh, size_t argc, char **argv) {
#if defined(CONFIG_SENSOR) && defined(CONFIG_AON_QDEC_BEE)
    static const struct gpio_dt_spec phase_a = GPIO_DT_SPEC_GET(DT_ALIAS(test_qenca), gpios);
    static const struct gpio_dt_spec phase_b = GPIO_DT_SPEC_GET(DT_ALIAS(test_qencb), gpios);
    static bool toggle_a;
    struct sensor_value val;
    const struct device *const dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(test_aon_qdec));

    gpio_pin_configure_dt(&phase_a, GPIO_OUTPUT);
    gpio_pin_configure_dt(&phase_b, GPIO_OUTPUT);

    k_busy_wait(100000);

    for (int i = 0; i < 10; i++) {
        toggle_a = !toggle_a;
        if (toggle_a) {
            gpio_pin_toggle_dt(&phase_a);
        } else {
            gpio_pin_toggle_dt(&phase_b);
        }
        k_busy_wait(100000);
        sensor_sample_fetch(dev);
        sensor_channel_get(dev, SENSOR_CHAN_ROTATION, &val);
        printf("Position[%d] = %d degrees\n", i, val.val1);
    }

    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    printf("[%lld] before enter dlps\n", k_uptime_get());
    printf("[%lld] type on shell to wakeup\n", k_uptime_get());
    dlps_check_flag = PM_CHECK_PASS;
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_FOREVER);

    dlps_check_flag = PM_CHECK_FAIL;

    /* rx 1 byte to wakeup */
    DBG_DIRECT("[%s] wakeup", __func__);
    printf("[%lld] after enter dlps\n", k_uptime_get());

    for (int i = 0; i < 10; i++) {
        toggle_a = !toggle_a;
        if (!toggle_a) {
            gpio_pin_toggle_dt(&phase_a);
        } else {
            gpio_pin_toggle_dt(&phase_b);
        }
        k_busy_wait(100000);
        sensor_sample_fetch(dev);
        sensor_channel_get(dev, SENSOR_CHAN_ROTATION, &val);
        printf("Position[%d] = %d degrees\n", i, val.val1);
    }

    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    printf("[%lld] before enter dlps\n", k_uptime_get());
    printf("[%lld] type on shell to wakeup\n", k_uptime_get());
    dlps_check_flag = PM_CHECK_PASS;
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_FOREVER);

    dlps_check_flag = PM_CHECK_FAIL;

    /* rx 1 byte to wakeup */
    DBG_DIRECT("[%s] wakeup", __func__);
    printf("[%lld] after enter dlps\n", k_uptime_get());

    for (int i = 0; i < 10; i++) {
        toggle_a = !toggle_a;
        if (!toggle_a) {
            gpio_pin_toggle_dt(&phase_a);
        } else {
            gpio_pin_toggle_dt(&phase_b);
        }
        k_busy_wait(100000);
        sensor_sample_fetch(dev);
        sensor_channel_get(dev, SENSOR_CHAN_ROTATION, &val);
        printf("Position[%d] = %d degrees\n", i, val.val1);
    }

    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    printf("[%lld] before enter dlps\n", k_uptime_get());
    printf("[%lld] type on shell to wakeup\n", k_uptime_get());
    dlps_check_flag = PM_CHECK_PASS;
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_FOREVER);

    dlps_check_flag = PM_CHECK_FAIL;

    /* rx 1 byte to wakeup */
    DBG_DIRECT("[%s] wakeup", __func__);
    printf("[%lld] after enter dlps\n", k_uptime_get());

    for (int i = 0; i < 10; i++) {
        toggle_a = !toggle_a;
        if (toggle_a) {
            gpio_pin_toggle_dt(&phase_a);
        } else {
            gpio_pin_toggle_dt(&phase_b);
        }
        k_busy_wait(100000);
        sensor_sample_fetch(dev);
        sensor_channel_get(dev, SENSOR_CHAN_ROTATION, &val);
        printf("Position[%d] = %d degrees\n", i, val.val1);
    }
#endif
    return 0;
}

static int shell_pm_test_i2c(const struct shell *sh, size_t argc, char **argv) {
#ifdef CONFIG_I2C
    const struct device *const i2c_dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(i2c0));
    unsigned char icm20618_addr = 0x68;
    unsigned char write_buf[6], read_buf[12];
    int write_len, read_len;
    (void)memset(write_buf, 0, sizeof(write_buf));
    (void)memset(read_buf, 0, sizeof(read_buf));
    /* read id */
    write_buf[0] = 0x00;
    write_len = 1;
    read_len = 1;
    i2c_write_read(i2c_dev, icm20618_addr, write_buf, write_len, read_buf, read_len);
    printf("icm20618 addr:0x%x reg: 0x%x = 0x%x\n", icm20618_addr, write_buf[0], read_buf[0]);

    /* enter dlps */
    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    printf("[%lld] before enter dlps\n", k_uptime_get());
    printf("[%lld] type on shell to wakeup\n", k_uptime_get());
    dlps_check_flag = PM_CHECK_PASS;
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_FOREVER);

    dlps_check_flag = PM_CHECK_FAIL;

    /* rx 1 byte to wakeup */
    DBG_DIRECT("[%s] wakeup", __func__);
    printf("[%lld] after enter dlps\n", k_uptime_get());
    (void)memset(write_buf, 0, sizeof(write_buf));
    (void)memset(read_buf, 0, sizeof(read_buf));
    /* read id */
    write_buf[0] = 0x00;
    write_len = 1;
    read_len = 1;
    i2c_write_read(i2c_dev, icm20618_addr, write_buf, write_len, read_buf, read_len);
    printf("icm20618 addr:0x%x reg: 0x%x = 0x%x\n", icm20618_addr, write_buf[0], read_buf[0]);

#endif
    return 0;
}

#ifdef CONFIG_ADC
#define ADC_BUFFER_SIZE 2
#define INVALID_ADC_VALUE SHRT_MIN

#define DT_SPEC_AND_COMMA(node_id, prop, idx) ADC_DT_SPEC_GET_BY_IDX(node_id, idx),
static const struct adc_dt_spec adc_channels[] = {
    DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels, DT_SPEC_AND_COMMA)};

static const int adc_channels_count = ARRAY_SIZE(adc_channels);

static int32_t m_sample_buffer[ADC_BUFFER_SIZE];
static uint8_t m_samplings_done;

static void check_samples(int expected_count) {
    printf("Samples read: ");
    for (int i = 0; i < ADC_BUFFER_SIZE; i++) {
        int32_t sample_value = m_sample_buffer[i];

        printf("[%u]:0x%04hx ", i, sample_value);
        if (i < expected_count) {
            if (INVALID_ADC_VALUE == sample_value) {
                printf("[%u]:0x%04hx should be filled ", i, sample_value);
            }
        } else {
            if (INVALID_ADC_VALUE != sample_value) {
                printf("[%u]:0x%04hx should be %d ", i, sample_value, INVALID_ADC_VALUE);
            }
        }
    }
    printf("\n");
}

static enum adc_action repeated_samplings_callback(const struct device *dev,
                                                   const struct adc_sequence *sequence,
                                                   uint16_t sampling_index) {
    ++m_samplings_done;
    printf("%s: done %d\n", __func__, m_samplings_done);
    if (m_samplings_done == 1U) {
        check_samples(MIN(adc_channels_count, 2));

        /* After first sampling continue normally. */
        return ADC_ACTION_CONTINUE;
    } else {
        check_samples(2 * MIN(adc_channels_count, 2));

        /*
         * The second sampling is repeated 9 times (the samples are
         * written in the same place), then the sequence is finished
         * prematurely.
         */
        if (m_samplings_done < 10) {
            return ADC_ACTION_REPEAT;
        } else {
            return ADC_ACTION_FINISH;
        }
    }
}
#endif

static int shell_pm_test_adc(const struct shell *sh, size_t argc, char **argv) {
#ifdef CONFIG_ADC
    const struct adc_sequence_options options = {
        .callback = repeated_samplings_callback,
        .extra_samplings = 2,
        .interval_us = 0,
    };
    struct adc_sequence sequence = {
        .options = &options,
        .buffer = m_sample_buffer,
        .buffer_size = sizeof(m_sample_buffer),
        .resolution = 12,
    };

    for (uint8_t i = 0; i < adc_channels_count; i++) {
        adc_channel_setup_dt(&adc_channels[i]);
    }

    (void)adc_sequence_init_dt(&adc_channels[0], &sequence);
    printf("adc_channels_count=%d, adc_channels[0].channel_id=%d\n", adc_channels_count,
           adc_channels[0].channel_id);
    if (adc_channels_count > 1) {
        sequence.channels |= BIT(adc_channels[1].channel_id);
    }

    for (uint8_t i = 0; i < ADC_BUFFER_SIZE; ++i) {
        m_sample_buffer[i] = INVALID_ADC_VALUE;
    }

    m_samplings_done = 0;

    adc_read_dt(&adc_channels[0], &sequence);

    /* enter dlps */
    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    printf("[%lld] before enter dlps\n", k_uptime_get());
    printf("[%lld] type on shell to wakeup\n", k_uptime_get());
    dlps_check_flag = PM_CHECK_PASS;
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_FOREVER);

    dlps_check_flag = PM_CHECK_FAIL;

    /* rx 1 byte to wakeup */
    DBG_DIRECT("[%s] wakeup", __func__);
    printf("[%lld] after enter dlps\n", k_uptime_get());

    for (uint8_t i = 0; i < ADC_BUFFER_SIZE; ++i) {
        m_sample_buffer[i] = INVALID_ADC_VALUE;
    }

    m_samplings_done = 0;

    adc_read_dt(&adc_channels[0], &sequence);

#endif

    return 0;
}

static int shell_pm_test_sdhc(const struct shell *sh, size_t argc, char **argv) {
#ifdef CONFIG_SDMMC_STACK
    static const struct device *const sdhc_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(test_sdhc0));
    static struct sd_card card;
    int ret;
    printf("Card initializing...\n");
    ret = sd_init(sdhc_dev, &card);

    if (ret != 0) {
        printf("Card initialization failed\n");
        return 0;
    }

    printf("Card initialization success\n");

    /* enter dlps */
    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    printf("[%lld] before enter dlps\n", k_uptime_get());
    printf("[%lld] type on shell to wakeup\n", k_uptime_get());
    dlps_check_flag = PM_CHECK_PASS;
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_FOREVER);

    dlps_check_flag = PM_CHECK_FAIL;

    /* rx 1 byte to wakeup */
    DBG_DIRECT("[%s] wakeup", __func__);
    printf("[%lld] after enter dlps\n", k_uptime_get());

    printf("Card initializing...\n");
    ret = sd_init(sdhc_dev, &card);

    if (ret != 0) {
        printf("Card initialization failed\n");
        return 0;
    }

    printf("Card initialization success\n");
#endif
    return 0;
}

#ifdef CONFIG_CAN
static volatile bool can_rx_received = false;

static void can_tx_callback(const struct device *dev, int error, void *user_data) {
    printf("dev %s tx cb\n", dev->name);
}

static void can_rx_callback(const struct device *dev, struct can_frame *frame, void *user_data) {
    printf("dev %s rx cb reecive id:0x%x, %ddata: ", dev->name, frame->id, frame->dlc);

    for (uint8_t i = 0; i < frame->dlc; i++) {
        printf("0x%02x ", frame->data[i]);
    }

    printf("\n");

    can_rx_received = true;
}
#endif

static int shell_pm_test_can(const struct shell *sh, size_t argc, char **argv) {
#ifdef CONFIG_CAN
    static const struct device *const can_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(test_can));
    int ret;
    struct can_frame frame = {0};
    struct can_filter filter;

    frame.flags = 0;
    frame.dlc = 0;
    frame.id = 0x123;
    frame.dlc = 8;

    filter.flags = 0U;
    filter.id = 0x456;
    filter.mask = 0x3ff;

    for (uint8_t i = 0; i < frame.dlc; i++) {
        frame.data[i] = i;
    }

    ret = can_start(can_dev);
    if (ret != 0) {
        printf("failed to start CAN controller (ret %d)\n", ret);
        return ret;
    }

    can_send(can_dev, &frame, K_NO_WAIT, can_tx_callback, NULL);

    k_busy_wait(10000);

    can_rx_received = false;
    can_add_rx_filter(can_dev, can_rx_callback, NULL, &filter);

    printf("waiting for frame with id 0x456 from tool\n");
    while (can_rx_received == false)
        ;

    /* enter dlps */
    DBG_DIRECT("[%s] dlps_check_flag = 1", __func__);
    printf("[%lld] before enter dlps\n", k_uptime_get());
    printf("[%lld] type on shell to wakeup\n", k_uptime_get());
    dlps_check_flag = PM_CHECK_PASS;
    k_sem_init(&app_sem, 0, 1);
    k_sem_take(&app_sem, K_FOREVER);

    dlps_check_flag = PM_CHECK_FAIL;

    /* rx 1 byte to wakeup */
    DBG_DIRECT("[%s] wakeup", __func__);
    printf("[%lld] after enter dlps\n", k_uptime_get());

    can_send(can_dev, &frame, K_NO_WAIT, can_tx_callback, NULL);

    k_busy_wait(10000);

    can_rx_received = false;
    can_add_rx_filter(can_dev, can_rx_callback, NULL, &filter);

    printf("waiting for frame with id 0x456 from tool\n");
    while (can_rx_received == false)
        ;

    ret = can_stop(can_dev);
    if (ret != 0) {
        printf("failed to stop CAN controller (ret %d)\n", ret);
        return ret;
    }
#endif
    return 0;
}
#endif /* CONFIG_PM_DEVICE */

#if CONFIG_PM_DEVICE
#define SHELL_CMD_ARG_CREATE                                                                       \
    SHELL_CMD_ARG(uart, NULL, "uart pm test", shell_pm_test_uart, 0, 0),                           \
        SHELL_CMD_ARG(gpio, NULL, "gpio pm test", shell_pm_test_gpio, 0, 0),                       \
        SHELL_CMD_ARG(pwm, NULL, "pwm pm test", shell_pm_test_pwm, 0, 0),                          \
        SHELL_CMD_ARG(counter, NULL, "counter pm test(input a time in ms)", shell_pm_test_counter, \
                      2, 0),                                                                       \
        SHELL_CMD_ARG(spi, NULL, "spi pm test", shell_pm_test_spi, 0, 0),                          \
        SHELL_CMD_ARG(rtc, NULL, "rtc pm test", shell_pm_test_rtc, 0, 0),                          \
        SHELL_CMD_ARG(qdec, NULL, "qdec pm test", shell_pm_test_qdec, 0, 0),           \
        SHELL_CMD_ARG(aon_qdec, NULL, "aon_qdec pm test", shell_pm_test_aon_qdec, 0, 0),           \
        SHELL_CMD_ARG(i2c, NULL, "i2c pm test", shell_pm_test_i2c, 0, 0),                          \
        SHELL_CMD_ARG(adc, NULL, "adc pm test", shell_pm_test_adc, 0, 0),                          \
        SHELL_CMD_ARG(sdhc, NULL, "sdhc pm test", shell_pm_test_sdhc, 0, 0),                       \
        SHELL_CMD_ARG(can, NULL, "can pm test", shell_pm_test_can, 0, 0),                          \
        SHELL_SUBCMD_SET_END /* Array terminated. */
#else
#define SHELL_CMD_ARG_CREATE                                                                       \
    SHELL_CMD_ARG(uart, NULL, "Uart pm test", shell_pm_test_uart, 0, 0),                           \
        SHELL_CMD_ARG(counter, NULL, "Counter pm test(input a time in ms)", shell_pm_test_counter, \
                      2, 0),                                                                       \
        SHELL_SUBCMD_SET_END /* Array terminated. */

#endif
SHELL_STATIC_SUBCMD_SET_CREATE(sub_pm_test, SHELL_CMD_ARG_CREATE);

SHELL_CMD_REGISTER(pm_test, &sub_pm_test, "Pm test", NULL);
