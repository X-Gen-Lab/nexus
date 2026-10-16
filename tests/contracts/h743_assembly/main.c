/**
 * \file            main.c
 * \brief           H743 link-only selected factory and actual kernel consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/memory.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#ifdef NX_H743_FREERTOS_FIXTURE
#include "nexus/os/freertos.h"
static nx_freertos_task_t s_task;
_Alignas(8) static StackType_t s_task_stack[128];
static void task_entry(void* context) {
    (void)context;
}
#endif
#ifdef NX_H743_IO_FIXTURE
static nx_uart_tx_request_t s_request;
#ifdef NX_H743_DMA_FIXTURE
_Alignas(32) static uint8_t s_data[32] = {0x12U, 0x34U};
#else
static uint8_t s_data[2] = {0x12U, 0x34U};
#endif
#endif
#ifdef NX_H743_RX_FIXTURE
#if defined(NEXUS_MEMORY_SRAM1) && NEXUS_MEMORY_SRAM1 == 1
NX_MEMORY_SRAM1_BSS _Alignas(32) static uint8_t s_rx_data[2][64];
#else
_Alignas(32) static uint8_t s_rx_data[2][64];
#endif
static nx_stream_slot_t s_rx_slots[2] = {
    {.data = s_rx_data[0], .capacity = sizeof(s_rx_data[0])},
    {.data = s_rx_data[1], .capacity = sizeof(s_rx_data[1])}};
static nx_stream_t s_rx_stream;
#endif
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS) {
        return (int)start.primary;
    }
#ifdef NX_H743_PERIPHERAL_FIXTURE
    nx_adc_info_t adc;
    nx_pwm_state_t pwm;
    nx_watchdog_state_t watchdog;
    nx_exti_event_t edge;
    size_t received = 0U;
    (void)nx_pwm_port_state(nx_factory_pwm(NX_PWM_ID_WAVE3), &pwm);
    (void)nx_watchdog_port_state(nx_factory_watchdog(NX_WATCHDOG_ID_WATCHDOG0),
                                 &watchdog);
    (void)nx_exti_port_read(nx_factory_exti(NX_EXTI_ID_EDGE0), &edge, 1U,
                            &received);
    (void)nx_spi_endpoint_on_port(nx_device_device4,
                                  nx_factory_spi(NX_SPI_ID_BUS4));
    (void)nx_i2c_endpoint_on_port(nx_device_device1,
                                  nx_factory_i2c(NX_I2C_ID_BUS1));
    (void)nx_adc_port_info(nx_factory_adc(NX_ADC_ID_SENSOR), &adc);
    (void)nx_flash_port_geometry(nx_factory_flash(NX_FLASH_ID_STORAGE));
#endif
#ifdef NX_H743_IO_FIXTURE
    const nx_gpio_port_t* led = nx_factory_gpio(NX_GPIO_ID_LED);
    const nx_uart_port_t* console = nx_factory_uart(NX_UART_ID_CONSOLE);
#ifdef NX_H743_RX_FIXTURE
    (void)nx_stream_initialize(&s_rx_stream, s_rx_slots, 2U);
    (void)nx_uart_port_rx_start(console, &s_rx_stream);
#endif
    nx_request_initialize(&s_request.base);
    (void)nx_gpio_port_write(led, 1U << 0U, 0U);
    (void)nx_uart_tx_prepare(&s_request, s_data, sizeof(s_data),
                             nx_deadline_after(nx_time_now_us(), 1000U));
    (void)nx_uart_port_submit(console, &s_request);
    (void)nx_uart_port_cancel(console, &s_request);
    nx_uart_port_service(console);
#ifdef NX_H743_RX_FIXTURE
    (void)nx_uart_port_rx_stop(console);
#endif
#endif
#ifdef NX_H743_FREERTOS_FIXTURE
    /* Retain the real static kernel graph. This ELF is not executed by HIL. */
    (void)nx_freertos_task_start(&s_task, "h7-link", s_task_stack, 128U, 1U,
                                 task_entry, NULL);
    vTaskStartScheduler();
    /* A returning scheduler may have run tasks. Never roll it back here. */
    for (;;) {
    }
#else
    return (int)nx_platform_stop();
#endif
}
