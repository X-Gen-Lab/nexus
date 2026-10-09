/**
 * \file            provider.h
 *
 * \brief           Private fixed Native instance symbols for static assembly.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_NATIVE_PROVIDER_H
#define NEXUS_NATIVE_PROVIDER_H
#include "nexus/io/native/model.h"
extern nx_gpio_port_t g_nx_native_gpio;
extern nx_uart_port_t g_nx_native_uart;
extern nx_spi_endpoint_t g_nx_native_spi;
extern nx_i2c_port_t g_nx_native_i2c_port;
extern nx_i2c_endpoint_t g_nx_native_i2c;
extern nx_flash_port_t g_nx_native_flash;
extern nx_watchdog_port_t g_nx_native_watchdog;
extern nx_exti_port_t g_nx_native_exti;
extern nx_pwm_port_t g_nx_native_pwm;
extern nx_adc_port_t g_nx_native_adc;
#endif
