/**
 * \file            gd32_io_test.c
 * \brief           Fault and boundary tests against production GD32 providers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define _GNU_SOURCE
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
#include "private/system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

extern uint64_t g_gd32_model_now;
extern bool g_gd32_model_isr;
extern bool g_gd32_model_reset_fails;
extern bool g_gd32_model_adc_works;
extern bool g_gd32_model_i2c_stop_works;
extern bool g_gd32_model_fwdgt_config_fails;
extern bool g_gd32_model_debug_freeze_fails;
extern bool g_gd32_model_tc_before_critical;
extern uint32_t g_gd32_model_pulses;
extern uint32_t g_gd32_model_barrier_advance;
extern uint32_t g_gd32_reset_flags;
void USART0_IRQHandler(void);
void EXTI3_IRQHandler(void);
void EXTI5_9_IRQHandler(void);
static unsigned s_checks;

/** \brief           Keep checks effective even under NDEBUG. */
static void check(bool value, const char* expression, unsigned line) {
    ++s_checks;
    if (!value) {
        fprintf(stderr, "GD32 model check %u failed: %s\n", line, expression);
        exit(1);
    }
}
#define CHECK(expression) check((expression), #expression, __LINE__)

/** \brief           Map exact MCU addresses for a host MMIO behavior model. */
static void map_region(uintptr_t address, size_t size) {
    void* mapping =
        mmap((void*)address, size, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    CHECK(mapping == (void*)address);
}

/** \brief           Verify one-register batching and no write on rejection. */
static void gpio_cases(void) {
    nx_gpio_port_t port;
    g_gd32_model_isr = true;
    CHECK(nx_gd32_gpio_initialize(&port, 3u, 0x180u, 0u, true) ==
          NX_ERROR_CONTEXT);
    g_gd32_model_isr = false;
    CHECK(nx_gd32_gpio_initialize(&port, 3u, 0x180u, 0u, true) == NX_SUCCESS);
    CHECK(nx_gpio_port_write(&port, 0x80u, 0x100u) == NX_SUCCESS);
    CHECK(GPIO_BOP(GPIOD) == 0x01000080u);
    CHECK(nx_gpio_port_write(&port, 0x80u, 0x80u) == NX_ERROR_INVALID);
    CHECK(GPIO_BOP(GPIOD) == 0x01000080u);
    CHECK(nx_gpio_port_write(&port, 1u, 0u) == NX_ERROR_PERMISSION);
    CHECK(GPIO_BOP(GPIOD) == 0x01000080u);
    GPIO_ISTAT(GPIOD) = 0xFFFFu;
    uint32_t value = 0u;
    CHECK(nx_gpio_port_read(&port, &value) == NX_SUCCESS && value == 0x180u);
    GPIO_OCTL(GPIOD) = 0x80u;
    CHECK(nx_gpio_port_toggle(&port, 0x180u) == NX_SUCCESS);
    CHECK(GPIO_BOP(GPIOD) == 0x00800100u);
    CHECK(nx_gpio_port_toggle(&port, 1u) == NX_ERROR_PERMISSION);
    CHECK(nx_gd32_gpio_initialize(&port, 9u, 1u, 0u, true) == NX_ERROR_INVALID);
    CHECK(nx_gd32_gpio_stop(&port, 0u) == NX_SUCCESS);
    CHECK(GPIO_BOP(GPIOD) == 0x01800000u);
    CHECK(nx_gpio_port_write(&port, 0x80u, 0u) == NX_ERROR_INVALID);
}

/** \brief           Exercise true TC, rejected borrow, failed abort and late
 * IRQ. */
static void uart_cases(void) {
    nx_uart_port_t port;
    nx_uart_rx_event_t queue[2];
    CHECK(nx_gd32_uart_initialize(&port, 1200u, NX_UART_RX_EVENTS, queue, 2u,
                                  6u) == NX_ERROR_INVALID);
    CHECK(nx_gd32_uart_initialize(&port, 1525u, NX_UART_RX_EVENTS, queue, 2u,
                                  6u) == NX_ERROR_INVALID);
    CHECK(nx_gd32_uart_initialize(&port, 115200u, NX_UART_RX_EVENTS, queue, 2u,
                                  6u) == NX_SUCCESS);
    nx_uart_port_t duplicate;
    CHECK(nx_gd32_uart_initialize(&duplicate, 115200u, NX_UART_RX_EVENTS, queue,
                                  2u, 6u) == NX_ERROR_BUSY);
    uint8_t data[] = {0x11u, 0x22u};
    nx_uart_tx_request_t request;
    nx_request_initialize(&request.base);
    CHECK(nx_uart_tx_prepare(&request, data, 2u, g_gd32_model_now + 1000u) ==
          NX_SUCCESS);
    uint32_t saved_mask = nx_gd32_critical_enter();
    CHECK(nx_uart_port_submit(&port, &request) == NX_ERROR_CONTEXT);
    CHECK(nx_request_state(&request.base) == NX_REQUEST_READY);
    nx_gd32_critical_leave(saved_mask);
    CHECK(nx_uart_port_submit(&port, &request) == NX_SUCCESS);
    nx_uart_tx_request_t rejected;
    nx_request_initialize(&rejected.base);
    CHECK(nx_uart_tx_prepare(&rejected, data, 1u, g_gd32_model_now + 1000u) ==
          NX_SUCCESS);
    CHECK(nx_uart_port_submit(&port, &rejected) == NX_ERROR_BUSY);
    CHECK(nx_request_state(&rejected.base) == NX_REQUEST_READY);
    USART_STAT0(USART0) = USART_STAT0_TBE | USART_STAT0_TC;
    USART0_IRQHandler();
    CHECK(port.tx_position == 1u && !port.tc);
    USART_STAT0(USART0) = USART_STAT0_TBE;
    USART0_IRQHandler();
    CHECK(port.tx_position == 2u && !port.tc);
    nx_uart_port_service(&port);
    CHECK(nx_request_state(&request.base) == NX_REQUEST_ACTIVE);
    USART_STAT0(USART0) = USART_STAT0_TBE | USART_STAT0_TC;
    USART0_IRQHandler();
    nx_uart_port_service(&port);
    nx_result_t result;
    size_t transferred;
    CHECK(nx_request_result(&request.base, &result, &transferred) ==
          NX_SUCCESS);
    CHECK(result == NX_SUCCESS && transferred == 2u && port.active == NULL);

    /* The IRQ observation decides the deadline, not delayed task service. */
    CHECK(nx_uart_tx_prepare(&request, data, 1u, g_gd32_model_now + 20u) ==
          NX_SUCCESS);
    CHECK(nx_uart_port_submit(&port, &request) == NX_SUCCESS);
    USART_STAT0(USART0) = USART_STAT0_TBE;
    USART0_IRQHandler();
    USART_STAT0(USART0) = USART_STAT0_TC;
    USART0_IRQHandler();
    g_gd32_model_now += 30u;
    CHECK(nx_uart_port_cancel(&port, &request) == NX_SUCCESS);
    nx_uart_port_service(&port);
    CHECK(nx_request_result(&request.base, &result, &transferred) ==
          NX_SUCCESS);
    CHECK(result == NX_SUCCESS && transferred == 1u);
    /* A pending TC preempts immediately before cancel masks interrupts. */
    CHECK(nx_uart_tx_prepare(&request, data, 1u, g_gd32_model_now + 1000u) ==
          NX_SUCCESS);
    CHECK(nx_uart_port_submit(&port, &request) == NX_SUCCESS);
    USART_STAT0(USART0) = USART_STAT0_TBE;
    USART0_IRQHandler();
    g_gd32_model_tc_before_critical = true;
    CHECK(nx_uart_port_cancel(&port, &request) == NX_SUCCESS);
    CHECK(!g_gd32_model_tc_before_critical && port.tc);
    CHECK(!nx_gd32_irq_masked());
    nx_uart_port_service(&port);
    CHECK(nx_request_result(&request.base, &result, &transferred) ==
          NX_SUCCESS);
    CHECK(result == NX_SUCCESS && transferred == 1u);

    CHECK(nx_uart_tx_prepare(&request, data, 1u, g_gd32_model_now + 20u) ==
          NX_SUCCESS);
    CHECK(nx_uart_port_submit(&port, &request) == NX_SUCCESS);
    USART_STAT0(USART0) = USART_STAT0_TBE;
    USART0_IRQHandler();
    g_gd32_model_now += 30u;
    USART_STAT0(USART0) = USART_STAT0_TC;
    USART0_IRQHandler();
    nx_uart_port_service(&port);
    CHECK(nx_request_result(&request.base, &result, &transferred) ==
          NX_SUCCESS);
    CHECK(result == NX_ERROR_TIMEOUT && transferred == 1u);
    USART0_IRQHandler();
    CHECK(port.active == NULL);

    CHECK(nx_uart_tx_prepare(&request, data, 2u, g_gd32_model_now + 1000u) ==
          NX_SUCCESS);
    CHECK(nx_uart_port_submit(&port, &request) == NX_SUCCESS);
    CHECK(nx_uart_port_cancel(&port, &request) == NX_SUCCESS);
    g_gd32_model_reset_fails = true;
    nx_uart_port_service(&port);
    CHECK(nx_request_state(&request.base) == NX_REQUEST_QUARANTINED);
    CHECK(port.active == &request);
    CHECK(nx_uart_port_stop(&port) == NX_ERROR_BUSY);
    CHECK(nx_uart_tx_prepare(&request, data, 1u, 1000u) == NX_ERROR_STATE);
    g_gd32_model_reset_fails = false;
    nx_uart_port_service(&port);
    CHECK(nx_request_result(&request.base, &result, &transferred) ==
          NX_SUCCESS);
    CHECK(result == NX_ERROR_CANCELLED && port.active == NULL);
    CHECK(nx_uart_port_stop(&port) == NX_SUCCESS);
    USART0_IRQHandler();

    CHECK(nx_gd32_uart_initialize(&port, 115200u, NX_UART_RX_EVENTS, queue, 2u,
                                  6u) == NX_SUCCESS);
    for (unsigned i = 0u; i < 3u; ++i) {
        USART_STAT0(USART0) = USART_STAT0_RBNE;
        USART_DATA(USART0) = 0x30u + i;
        USART0_IRQHandler();
    }
    nx_uart_rx_event_t output[4];
    size_t count;
    CHECK(nx_uart_port_read_events(&port, output, 4u, &count) == NX_SUCCESS);
    CHECK(count == 3u && output[0].byte == 0x30u && output[1].byte == 0x31u);
    CHECK((output[2].flags & (NX_UART_EVENT_LOSS | NX_UART_EVENT_NO_BYTE)) ==
          (NX_UART_EVENT_LOSS | NX_UART_EVENT_NO_BYTE));
    USART_STAT0(USART0) = USART_STAT0_FERR;
    USART_DATA(USART0) = 0xCCu;
    USART0_IRQHandler();
    CHECK(nx_uart_port_read_events(&port, output, 4u, &count) == NX_SUCCESS);
    CHECK(count == 1u &&
          (output[0].flags & (NX_UART_EVENT_FRAMING | NX_UART_EVENT_NO_BYTE)) ==
              (NX_UART_EVENT_FRAMING | NX_UART_EVENT_NO_BYTE));
    CHECK(nx_uart_port_read_events(&port, output, 4u, &count) ==
          NX_ERROR_EMPTY);
    CHECK(nx_uart_tx_prepare(&request, data, 1u, g_gd32_model_now + 3u) ==
          NX_SUCCESS);
    CHECK(nx_uart_port_submit(&port, &request) == NX_SUCCESS);
    g_gd32_model_now += 4u;
    nx_uart_port_service(&port);
    CHECK(nx_request_result(&request.base, &result, &transferred) ==
          NX_SUCCESS);
    CHECK(result == NX_ERROR_TIMEOUT && transferred == 0u);
    CHECK(nx_uart_port_stop(&port) == NX_SUCCESS);

    uint8_t bytes[1];
    CHECK(nx_gd32_uart_initialize(&port, 115200u, NX_UART_RX_BYTES, bytes, 1u,
                                  6u) == NX_SUCCESS);
    USART_STAT0(USART0) = USART_STAT0_RBNE;
    USART_DATA(USART0) = 0x55u;
    USART0_IRQHandler();
    USART_DATA(USART0) = 0x66u;
    USART0_IRQHandler();
    uint8_t byte = 0u;
    CHECK(nx_uart_port_read_bytes(&port, &byte, 1u, &count) ==
          NX_ERROR_OVERFLOW);
    CHECK(count == 1u && byte == 0x55u);
    USART_STAT0(USART0) = USART_STAT0_ORERR;
    USART_DATA(USART0) = 0xCCu;
    USART0_IRQHandler();
    byte = 0xEEu;
    CHECK(nx_uart_port_read_bytes(&port, &byte, 1u, &count) ==
          NX_ERROR_OVERFLOW);
    CHECK(count == 0u && byte == 0xEEu);

    CHECK(nx_uart_port_stop(&port) == NX_SUCCESS);
}

/** \brief           Verify poll failures release CS and leave no active owner.
 */
static void spi_cases(void) {
    nx_spi_port_t port;
    nx_spi_endpoint_t endpoint;
    CHECK(nx_gd32_spi_initialize(&port, &endpoint, 1000000u, 0u) == NX_SUCCESS);
    uint8_t tx[] = {0xA5u, 0x5Au};
    uint8_t rx[2] = {0u, 0u};
    size_t count;
    CHECK(nx_spi_endpoint_transfer(&endpoint, tx, rx, 257u,
                                   g_gd32_model_now + 100u,
                                   &count) == NX_ERROR_INVALID);
    CHECK(nx_spi_endpoint_transfer(&endpoint, tx, rx, 1u, NX_DEADLINE_NEVER,
                                   &count) == NX_ERROR_INVALID);
    SPI_STAT(SPI4) = SPI_STAT_TBE | SPI_STAT_RBNE;
    CHECK(nx_spi_endpoint_transfer(&endpoint, tx, rx, 2u,
                                   g_gd32_model_now + 100u,
                                   &count) == NX_SUCCESS);
    CHECK(count == 2u && rx[0] == tx[0] && rx[1] == tx[1]);
    CHECK(GPIO_BOP(GPIOF) == GPIO_PIN_6 && !port.active);
    /* Completion data remains valid when final CS cleanup passes deadline. */
    g_gd32_model_barrier_advance = 100u;
    CHECK(nx_spi_endpoint_transfer(&endpoint, tx, rx, 2u,
                                   g_gd32_model_now + 100u,
                                   &count) == NX_ERROR_TIMEOUT);
    CHECK(count == 2u && GPIO_BOP(GPIOF) == GPIO_PIN_6 && !port.active);
    g_gd32_model_barrier_advance = 0u;

    SPI_STAT(SPI4) = SPI_STAT_TBE;
    CHECK(nx_spi_endpoint_transfer(&endpoint, tx, rx, 2u, g_gd32_model_now + 3u,
                                   &count) == NX_ERROR_TIMEOUT);
    CHECK(count == 0u && GPIO_BOP(GPIOF) == GPIO_PIN_6 && !port.active);
    SPI_STAT(SPI4) = SPI_STAT_CONFERR;
    CHECK(nx_spi_endpoint_transfer(&endpoint, tx, rx, 2u,
                                   g_gd32_model_now + 100u,
                                   &count) == NX_ERROR_IO);
    CHECK(GPIO_BOP(GPIOF) == GPIO_PIN_6 && !port.active);
    port.active = true;
    CHECK(nx_spi_endpoint_transfer(&endpoint, tx, rx, 2u,
                                   g_gd32_model_now + 100u,
                                   &count) == NX_ERROR_BUSY);
    CHECK(nx_gd32_spi_stop(&port) == NX_ERROR_BUSY);
    port.active = false;
    CHECK(nx_gd32_spi_stop(&port) == NX_SUCCESS);
}

/** \brief           Exercise repeated START, limited reads and actual recovery
 * facts. */
static void i2c_cases(void) {
    nx_i2c_port_t port;
    nx_i2c_endpoint_t endpoint;
    CHECK(nx_gd32_i2c_initialize(&port, &endpoint, 0x50u, 400000u) ==
          NX_ERROR_UNSUPPORTED);
    CHECK(nx_gd32_i2c_initialize(&port, &endpoint, 0x50u, 100000u) ==
          NX_SUCCESS);
    uint8_t tx[] = {0x12u, 0x34u};
    uint8_t rx[3] = {0xEEu, 0xEEu, 0xEEu};
    nx_i2c_message_t messages[] = {{tx, 2u, false}, {rx, 2u, true}};
    size_t count;
    g_gd32_model_i2c_stop_works = true;
    I2C_STAT0(I2C0) = I2C_STAT0_SBSEND | I2C_STAT0_ADDSEND | I2C_STAT0_TBE |
                      I2C_STAT0_BTC | I2C_STAT0_RBNE;
    CHECK(nx_i2c_endpoint_transaction(&endpoint, messages, 2u,
                                      g_gd32_model_now + 100u,
                                      &count) == NX_SUCCESS);
    CHECK(count == 4u && rx[2] == 0xEEu && !port.active);
    messages[1].length = 3u;
    CHECK(nx_i2c_endpoint_transaction(&endpoint, messages, 2u,
                                      g_gd32_model_now + 100u,
                                      &count) == NX_ERROR_UNSUPPORTED);
    messages[1].length = 2u;
    I2C_STAT0(I2C0) = I2C_STAT0_AERR;
    CHECK(nx_i2c_endpoint_transaction(&endpoint, messages, 2u,
                                      g_gd32_model_now + 100u,
                                      &count) == NX_ERROR_NACK);
    CHECK(count == 0u && port.faulted && !port.active);
    CHECK(nx_i2c_endpoint_transaction(&endpoint, messages, 2u,
                                      g_gd32_model_now + 100u,
                                      &count) == NX_ERROR_STATE);
    GPIO_ISTAT(GPIOB) = GPIO_PIN_6;
    CHECK(nx_i2c_port_recover(&port) == NX_ERROR_IO && port.faulted);
    GPIO_ISTAT(GPIOB) = GPIO_PIN_6 | GPIO_PIN_7;
    CHECK(nx_i2c_port_recover(&port) == NX_SUCCESS && !port.faulted);
    I2C_STAT0(I2C0) = I2C_STAT0_LOSTARB;
    CHECK(nx_i2c_endpoint_transaction(&endpoint, messages, 2u,
                                      g_gd32_model_now + 100u,
                                      &count) == NX_ERROR_ARBITRATION);
    CHECK(port.faulted && !port.active);
    CHECK(nx_gd32_i2c_stop(&port) == NX_SUCCESS);
}

/** \brief           Check physical geometry, ranges and pulse-before-deadline
 * rules. */
static void flash_cases(void) {
    nx_flash_port_t port;
    *(uint32_t*)0x1FFF7A20u = 1024u << 16;
    CHECK(nx_gd32_flash_initialize(&port) == NX_SUCCESS);
    const nx_flash_geometry_t* geometry = nx_flash_port_geometry(&port);
    CHECK(geometry && geometry->size == 1048576u &&
          geometry->sector_count == 256u);
    CHECK(geometry->sectors[255].offset == 1044480u);
    uint8_t data[2] = {0x12u, 0x34u};
    uint8_t read[2] = {0u, 0u};
    CHECK(nx_flash_port_program(&port, 0u, data, 2u, g_gd32_model_now + 100u) ==
          NX_SUCCESS);
    CHECK(nx_flash_port_read(&port, 0u, read, 2u) == NX_SUCCESS);
    CHECK(memcmp(data, read, 2u) == 0);
    unsigned pulses = g_gd32_model_pulses;
    CHECK(nx_flash_port_program(&port, 1u, data, 2u, NX_DEADLINE_NEVER) ==
          NX_ERROR_INVALID);
    CHECK(nx_flash_port_read(&port, UINT32_MAX, read, 2u) == NX_ERROR_INVALID);
    CHECK(nx_flash_port_read(&port, 0u, read, SIZE_MAX) == NX_ERROR_INVALID);
    CHECK(nx_flash_port_erase(&port, 2048u, 4096u, NX_DEADLINE_NEVER) ==
          NX_ERROR_INVALID);
    CHECK(g_gd32_model_pulses == pulses);
    CHECK(nx_flash_port_erase(&port, 0u, 4096u, g_gd32_model_now) ==
          NX_ERROR_TIMEOUT);
    CHECK(g_gd32_model_pulses == pulses);
    CHECK(nx_flash_port_erase(&port, 0u, 4096u, g_gd32_model_now + 100u) ==
          NX_SUCCESS);
    CHECK(*(uint32_t*)0x08000000u == UINT32_MAX);
    nx_flash_region_t region = {&port, 1044480u, 4096u, false};
    CHECK(nx_flash_region_validate(&region) == NX_SUCCESS);
    CHECK(nx_flash_region_program(&region, 0u, data, 2u, NX_DEADLINE_NEVER) ==
          NX_ERROR_PERMISSION);
    region.writable = true;
    CHECK(nx_flash_region_program(&region, 4095u, data, 2u,
                                  NX_DEADLINE_NEVER) == NX_ERROR_INVALID);
    CHECK(nx_flash_region_program(&region, 0u, data, 2u, NX_DEADLINE_NEVER) ==
          NX_SUCCESS);
    CHECK((FMC_CTL & FMC_CTL_LK) != 0u);
    CHECK(nx_gd32_flash_stop(&port) == NX_SUCCESS);
    CHECK(nx_flash_port_geometry(&port) == NULL);
}

/** \brief           Preserve irreversible state and distinguish pre-enable
 * failure. */
static void watchdog_cases(bool late_failure) {
    nx_watchdog_port_t port;
    FMC_OBCTL0 = FMC_OBCTL0_NWDG_HW;
    CHECK(nx_gd32_watchdog_initialize(&port) == NX_SUCCESS);
    CHECK(nx_watchdog_port_feed(&port) == NX_ERROR_STATE);
    nx_watchdog_state_t state;
    RCU_RSTSCK |= RCU_RSTSCK_IRC32KSTB;
    g_gd32_model_fwdgt_config_fails = true;
    CHECK(nx_watchdog_port_enable(&port, 1000000u, false, &state) ==
          NX_ERROR_IO);
    CHECK(!state.enabled && !port.state.enabled);
    g_gd32_model_fwdgt_config_fails = false;
    g_gd32_model_debug_freeze_fails = late_failure;
    CHECK(nx_watchdog_port_enable(&port, 1000000u, true, &state) ==
          (late_failure ? NX_ERROR_IO : NX_SUCCESS));
    CHECK(state.enabled && state.irreversible &&
          state.debug_freeze == !late_failure);
    CHECK(state.minimum_timeout_us == 0u &&
          state.maximum_timeout_us == UINT32_MAX);
    CHECK(nx_watchdog_port_enable(&port, 1000000u, false, &state) ==
          NX_ERROR_STATE);
    CHECK(nx_watchdog_port_feed(&port) == NX_SUCCESS);
    CHECK(FWDGT_CTL == FWDGT_KEY_RELOAD);
    CHECK(nx_gd32_watchdog_stop(&port) == NX_ERROR_UNSUPPORTED);
    CHECK(port.initialized && port.state.enabled);
    g_gd32_reset_flags = RCU_RSTSCK_FWDGTRSTF | RCU_RSTSCK_BORRSTF;
    CHECK(nx_reset_cause() == (NX_RESET_IWDG | NX_RESET_BROWNOUT));
}

/** \brief           Verify duplicate/shared line conflicts, loss and stop
 * drain. */
static void exti_cases(void) {
    nx_exti_port_t port;
    nx_exti_event_t queue[1];
    CHECK(nx_gd32_exti_initialize(&port, 3u, 4u, NX_EXTI_BOTH, queue, 1u, 5u) ==
          NX_SUCCESS);
    nx_exti_port_t other;
    CHECK(nx_gd32_exti_initialize(&other, 3u, 0u, NX_EXTI_RISING, queue, 1u,
                                  5u) == NX_ERROR_BUSY);
    GPIO_ISTAT(GPIOE) = 8u;
    EXTI_PD = 8u;
    EXTI3_IRQHandler();
    EXTI_PD = 8u;
    EXTI3_IRQHandler();
    nx_exti_event_t output[2];
    size_t count;
    CHECK(nx_exti_port_read(&port, output, 2u, &count) == NX_SUCCESS);
    CHECK(count == 2u && output[0].line == 3u &&
          output[0].edge == NX_EXTI_RISING);
    CHECK((output[1].flags & NX_EXTI_EVENT_LOSS) != 0u);
    CHECK(nx_exti_port_stop(&port) == NX_SUCCESS);
    EXTI_PD = 8u;
    EXTI3_IRQHandler();
    CHECK(!port.initialized && port.count == 0u);
    nx_exti_port_t a;
    nx_exti_port_t b;
    nx_exti_event_t qa[1];
    nx_exti_event_t qb[1];
    CHECK(nx_gd32_exti_initialize(&a, 5u, 0u, NX_EXTI_RISING, qa, 1u, 5u) ==
          NX_SUCCESS);
    CHECK(nx_gd32_exti_initialize(&b, 7u, 1u, NX_EXTI_FALLING, qb, 1u, 6u) ==
          NX_ERROR_BUSY);
    CHECK(nx_gd32_exti_initialize(&b, 7u, 1u, NX_EXTI_FALLING, qb, 1u, 5u) ==
          NX_SUCCESS);
    EXTI_PD = (1u << 5) | (1u << 7);
    EXTI5_9_IRQHandler();
    CHECK(a.count == 1u && b.count == 1u &&
          qa[0].timestamp_us == qb[0].timestamp_us);
    CHECK(nx_exti_port_stop(&a) == NX_SUCCESS);
    CHECK((NVIC->ISER[(unsigned)EXTI5_9_IRQn / 32u] &
           (1u << ((unsigned)EXTI5_9_IRQn % 32u))) != 0u);
    CHECK(nx_exti_port_stop(&b) == NX_SUCCESS);
}

/** \brief           Check immutable PSC/ARR and exact zero/full CCR boundaries.
 */
static void pwm_cases(void) {
    nx_pwm_port_t port;
    CHECK(nx_gd32_pwm_initialize(&port, 1000u, 1000000u) == NX_SUCCESS);
    CHECK(nx_pwm_port_set(&port, 1000u, 1001u) == NX_ERROR_INVALID);
    CHECK(nx_pwm_port_set(&port, 999u, 1u) == NX_ERROR_STATE);
    CHECK(nx_pwm_port_set(&port, 1000u, 0u) == NX_SUCCESS);
    CHECK(TIMER_CH0CV(TIMER2) == 0u);
    CHECK(nx_pwm_port_set(&port, 1000u, 1000u) == NX_SUCCESS);
    CHECK(TIMER_CH0CV(TIMER2) == 1000u && TIMER_CAR(TIMER2) == 999u);
    CHECK(nx_pwm_port_start(&port) == NX_SUCCESS && port.state.running);
    CHECK(nx_pwm_port_stop(&port) == NX_SUCCESS && !port.state.running);
    CHECK(GPIO_BOP(GPIOA) == GPIO_PIN_6 << 16);
    CHECK(nx_pwm_port_start(&port) == NX_SUCCESS);
    CHECK(nx_gd32_pwm_release(&port) == NX_SUCCESS);
    CHECK(!port.initialized && !port.state.running);
    CHECK(nx_pwm_port_start(&port) == NX_ERROR_INVALID);
    CHECK(nx_gd32_pwm_initialize(&port, 1000u, 1000000u) == NX_SUCCESS);
    CHECK(nx_gd32_pwm_release(&port) == NX_SUCCESS);
}

/** \brief           Reject capacity before writes and preserve timeout prefix.
 */
static void adc_cases(void) {
    nx_adc_port_t port;
    uint8_t channels[] = {0u, 1u};
    g_gd32_model_adc_works = false;
    CHECK(nx_gd32_adc_initialize(&port, channels, 2u, 3300u,
                                 g_gd32_model_now + 20u) == NX_ERROR_TIMEOUT);
    g_gd32_model_adc_works = true;
    CHECK(nx_gd32_adc_initialize(&port, channels, 2u, 3300u,
                                 g_gd32_model_now + 100u) == NX_SUCCESS);
    uint16_t samples[3] = {0xAAAAu, 0xAAAAu, 0xAAAAu};
    size_t count;
    CHECK(nx_adc_port_sample(&port, samples, 1u, g_gd32_model_now + 100u,
                             &count) == NX_ERROR_INVALID);
    CHECK(samples[0] == 0xAAAAu);
    CHECK(nx_adc_port_sample(&port, samples, 3u, g_gd32_model_now + 100u,
                             &count) == NX_SUCCESS);
    CHECK(count == 2u && samples[0] == 123u && samples[1] == 124u);
    CHECK(samples[2] == 0xAAAAu);
    g_gd32_model_adc_works = false;
    CHECK(nx_adc_port_sample(&port, samples, 3u, g_gd32_model_now + 3u,
                             &count) == NX_ERROR_TIMEOUT);
    CHECK(count == 0u && !port.active &&
          (ADC_CTL1(ADC0) & ADC_CTL1_ADCON) == 0u);
    g_gd32_model_adc_works = true;
    CHECK(nx_adc_port_sample(&port, samples, 3u, g_gd32_model_now + 100u,
                             &count) == NX_SUCCESS);
    CHECK(nx_gd32_adc_stop(&port) == NX_SUCCESS);
}

/** \brief           Execute nonzero checks against linked production providers.
 */
int main(int argc, char** argv) {
    map_region(0x40000000u, 0x40000u);
    map_region(0x1FFF7000u, 0x2000u);
    map_region(0x08000000u, 1048576u);
    map_region(DBG_BASE, 0x1000u);
    memset((void*)0x08000000u, 0xFF, 1048576u);
    if (argc == 2 && strcmp(argv[1], "watchdog-late-failure") == 0) {
        watchdog_cases(true);
        printf("GD32 late watchdog failure model: %u checks passed\n",
               s_checks);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "watchdog-hardware-enabled") == 0) {
        nx_watchdog_port_t port;
        FMC_OBCTL0 = 0u;
        CHECK(nx_gd32_watchdog_initialize(&port) == NX_SUCCESS);
        nx_watchdog_state_t state;
        CHECK(nx_watchdog_port_state(&port, &state) == NX_SUCCESS);
        CHECK(state.enabled && state.irreversible);
        CHECK(FWDGT_CTL == 0u);
        CHECK(nx_gd32_watchdog_stop(&port) == NX_ERROR_UNSUPPORTED);
        CHECK(nx_watchdog_port_feed(&port) == NX_SUCCESS);
        CHECK(FWDGT_CTL == FWDGT_KEY_RELOAD);
        printf("GD32 hardware watchdog model: %u checks passed\n", s_checks);
        return 0;
    }
    CHECK(argc == 1);
    gpio_cases();
    uart_cases();
    spi_cases();
    i2c_cases();
    flash_cases();
    watchdog_cases(false);
    exti_cases();
    pwm_cases();
    adc_cases();
    printf("GD32 production provider host model: %u checks passed\n", s_checks);
    return 0;
}
