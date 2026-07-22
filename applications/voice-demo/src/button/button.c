/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <button.h>
#include <button_handle.h>
#include <ble/hid.h>
#include "rtl_pinmux.h"
#include "trace.h"

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);


static struct gpio_dt_spec test_pin1_irq = GPIO_DT_SPEC_GET(DT_NODELABEL(test_pin), test_pin1);
static struct gpio_dt_spec test_pin2_irq = GPIO_DT_SPEC_GET(DT_NODELABEL(test_pin), test_pin2);
static struct gpio_dt_spec test_pin3_irq = GPIO_DT_SPEC_GET(DT_NODELABEL(test_pin), test_pin3);
static struct gpio_dt_spec test_pin4_irq = GPIO_DT_SPEC_GET(DT_NODELABEL(test_pin), test_pin4);

static struct gpio_callback voice_test_pin1_cb;
static struct gpio_callback voice_test_pin2_cb;
static struct gpio_callback voice_test_pin3_cb;
static struct gpio_callback voice_test_pin4_cb;

static bool voice_button1_press = false;
static bool voice_button2_press = false;
static bool voice_button3_press = false;
static bool voice_button4_press = false;
static int voice_button_press_num = 0;

static void voice_test_button_callback(const struct device *dev, struct gpio_callback *gpio_cb,
                                      uint32_t pins) {
    if (!gpio_pin_get_raw(test_pin1_irq.port, test_pin1_irq.pin)) {
        voice_button1_press = true;
        voice_button_press_num++;
    } else if(!gpio_pin_get_raw(test_pin2_irq.port, test_pin2_irq.pin)) {
        voice_button2_press = true;
        voice_button_press_num++;
    }
    /* The dafault value of button3-P9_1, button4-P9_0 is always (0,1) and can not be changed when button is pressed, need check */
    //else if(!gpio_pin_get_raw(test_pin3_irq.port, test_pin3_irq.pin)) {
    //     voice_button3_press = true;
    //     voice_button_press_num++;
    // } else if(!gpio_pin_get_raw(test_pin4_irq.port, test_pin4_irq.pin)) {
    //     voice_button4_press = true;
    //     voice_button_press_num++;
    // }
    LOG_DBG("press button 1 %d, 2 %d", voice_button1_press, voice_button2_press);
    LOG_DBG("voice_test_button_callback, pins is %u, press button num %d,", pins, voice_button_press_num);

    if ((!strcmp(dev->name, test_pin1_irq.port->name)) && (pins & BIT(test_pin1_irq.pin))) {
        if(voice_button1_press) {
            voice_button1_press = false;
            gpio_pin_interrupt_configure_dt(&test_pin1_irq, GPIO_INT_LEVEL_HIGH);
        } else {
            voice_button_press_num--;
            gpio_pin_interrupt_configure_dt(&test_pin1_irq, GPIO_INT_LEVEL_LOW);

        }
    }
    if ((!strcmp(dev->name, test_pin2_irq.port->name)) && (pins & BIT(test_pin2_irq.pin))) {
        if(voice_button2_press) {
            voice_button2_press = false;
            gpio_pin_interrupt_configure_dt(&test_pin2_irq, GPIO_INT_LEVEL_HIGH);
        } else {
            voice_button_press_num--;
            gpio_pin_interrupt_configure_dt(&test_pin2_irq, GPIO_INT_LEVEL_LOW);
        }
    }
    // if ((!strcmp(dev->name, test_pin3_irq.port->name)) && (pins & BIT(test_pin3_irq.pin))) {
    //     if(voice_button3_press) {
    //         voice_button3_press = false;
    //         gpio_pin_interrupt_configure_dt(&test_pin3_irq, GPIO_INT_LEVEL_HIGH);
    //     } else {
    //         voice_button_press_num--;
    //         gpio_pin_interrupt_configure_dt(&test_pin3_irq, GPIO_INT_LEVEL_LOW);
    //     }
    // }
    // if ((!strcmp(dev->name, test_pin4_irq.port->name)) && (pins & BIT(test_pin4_irq.pin))) {
    //     if(voice_button4_press) {
    //         voice_button4_press = false;
    //         gpio_pin_interrupt_configure_dt(&test_pin4_irq, GPIO_INT_LEVEL_HIGH);
    //     } else {
    //         voice_button_press_num--;
    //         gpio_pin_interrupt_configure_dt(&test_pin4_irq, GPIO_INT_LEVEL_LOW);
    //     }
    // }

    if(voice_button_press_num > 0) {
        /* start voice init flow */
        key_handle_one_key_scenario();
    } else {
        key_handle_release_event();
    }
}

int voice_test_button_init(void) {

    LOG_DBG("voice_test_button_init");

    if (!(gpio_is_ready_dt(&test_pin1_irq) && gpio_is_ready_dt(&test_pin2_irq)
        && gpio_is_ready_dt(&test_pin3_irq) && gpio_is_ready_dt(&test_pin4_irq))) {
        LOG_ERR("voice test button device is not ready");
        return -ENODEV;
    }
    /* test button gpio callback enable and pin config */
    gpio_init_callback(&voice_test_pin1_cb, voice_test_button_callback, BIT((&test_pin1_irq)->pin));
    int rc = gpio_add_callback((&test_pin1_irq)->port, &voice_test_pin1_cb);
    if (rc != 0) {
        LOG_ERR("configure test_pin1_irq fail, err:%d ", rc);
    }
    gpio_pin_configure_dt(&test_pin1_irq, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure_dt(&test_pin1_irq, GPIO_INT_LEVEL_LOW);

    gpio_init_callback(&voice_test_pin2_cb, voice_test_button_callback, BIT((&test_pin2_irq)->pin));
    rc = gpio_add_callback((&test_pin2_irq)->port, &voice_test_pin2_cb);
    if (rc != 0) {
        LOG_ERR("configure test_pin2_irq fail, err:%d ", rc);
    }
    gpio_pin_configure_dt(&test_pin2_irq, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure_dt(&test_pin2_irq, GPIO_INT_LEVEL_LOW);

    // gpio_init_callback(&voice_test_pin3_cb, voice_test_button_callback, BIT((&test_pin3_irq)->pin));
    // rc = gpio_add_callback((&test_pin3_irq)->port, &voice_test_pin3_cb);
    // if (rc != 0) {
    //     LOG_ERR("configure test_pin3_irq fail, err:%d ", rc);
    // }
    // gpio_pin_configure_dt(&test_pin3_irq, GPIO_INPUT | GPIO_PULL_UP);
    // gpio_pin_interrupt_configure_dt(&test_pin3_irq, GPIO_INT_LEVEL_LOW);

    // gpio_init_callback(&voice_test_pin4_cb, voice_test_button_callback, BIT((&test_pin4_irq)->pin));
    // rc = gpio_add_callback((&test_pin4_irq)->port, &voice_test_pin4_cb);
    // if (rc != 0) {
    //     LOG_ERR("configure test_pin4_irq fail, err:%d ", rc);
    // }
    // gpio_pin_configure_dt(&test_pin4_irq, GPIO_INPUT | GPIO_PULL_UP);
    // gpio_pin_interrupt_configure_dt(&test_pin4_irq, GPIO_INT_LEVEL_LOW);

    return 0;

}