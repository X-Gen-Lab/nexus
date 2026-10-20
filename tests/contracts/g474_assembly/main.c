/**
 * \file            main.c
 * \brief           G474 link-only selected factory and actual kernel consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
#ifdef NX_G474_FREERTOS_FIXTURE
#include "nexus/os/freertos.h"
static nx_freertos_task_t s_task;
_Alignas(8) static StackType_t s_task_stack[128];
static void task_entry(void* context) {
    (void)context;
}
#endif
#ifdef NX_G474_IO_FIXTURE
static nx_uart_tx_request_t s_request;
static uint8_t s_data[2] = {0x12U, 0x34U};
#endif
#ifdef NX_G474_EXTI_FIXTURE
static nx_exti_event_t s_events[2];
#endif
/* Passive link consumers preserve selected faces and their ops without
 * starting PWM, enabling IWDG or issuing SPI transactions. */
static const void* volatile s_optional_face;
static void retain_optional_faces(void) {
    for (nx_pwm_id_t id = 0U; id != NX_PWM_ID_COUNT; ++id)
        s_optional_face = nx_factory_pwm(id);
    for (nx_watchdog_id_t id = 0U; id != NX_WATCHDOG_ID_COUNT; ++id)
        s_optional_face = nx_factory_watchdog(id);
    for (nx_spi_id_t id = 0U; id != NX_SPI_ID_COUNT; ++id)
        s_optional_face = nx_factory_spi(id);
    for (nx_spi_device_id_t id = 0U; id != NX_SPI_DEVICE_ID_COUNT; ++id)
        s_optional_face = nx_factory_spi_device(id);
    for (nx_adc_id_t id = 0U; id != NX_ADC_ID_COUNT; ++id)
        s_optional_face = nx_factory_adc(id);
    for (nx_i2c_id_t id = 0U; id != NX_I2C_ID_COUNT; ++id)
        s_optional_face = nx_factory_i2c(id);
    for (nx_i2c_device_id_t id = 0U; id != NX_I2C_DEVICE_ID_COUNT; ++id)
        s_optional_face = nx_factory_i2c_device(id);
    for (nx_flash_id_t id = 0U; id != NX_FLASH_ID_COUNT; ++id)
        s_optional_face = nx_factory_flash(id);
}
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS) {
        return (int)start.primary;
    }
    retain_optional_faces();
#ifdef NX_G474_IO_FIXTURE
    const nx_gpio_port_t* led = nx_factory_gpio(NX_GPIO_ID_LED);
    /* This block is selected only with UARTs. Generated assembly-local IDs
     * are compact and begin at zero, independently of authored names. */
    const nx_uart_port_t* console = nx_factory_uart((nx_uart_id_t)0U);
    nx_request_initialize(&s_request.base);
    (void)nx_gpio_port_write(led, 1U << 5U, 0U);
    (void)nx_uart_tx_prepare(&s_request, s_data, sizeof(s_data),
                             nx_deadline_after(nx_time_now_us(), 1000U));
    (void)nx_uart_port_submit(console, &s_request);
    (void)nx_uart_port_cancel(console, &s_request);
    nx_uart_port_service(console);
#endif
#ifdef NX_G474_EXTI_FIXTURE
    const nx_exti_port_t* edge0 = nx_factory_exti(NX_EXTI_ID_EDGE0);
    const nx_exti_port_t* edge1 = nx_factory_exti(NX_EXTI_ID_EDGE1);
    size_t count = 0U;
    (void)nx_exti_port_attach_wake(edge0, NULL, 0U);
    (void)nx_exti_port_read(edge0, s_events, 2U, &count);
    (void)nx_exti_port_read(edge1, s_events, 2U, &count);
#endif
#ifdef NX_G474_FREERTOS_FIXTURE
    /* Retain the real static kernel graph. This ELF is not executed by HIL. */
    (void)nx_freertos_task_start(&s_task, "g4-link", s_task_stack, 128U, 1U,
                                 task_entry, NULL);
    vTaskStartScheduler();
    /* A returning scheduler may have run tasks. Never roll it back here. */
    for (;;) {
    }
#else
    return (int)nx_platform_stop();
#endif
}
