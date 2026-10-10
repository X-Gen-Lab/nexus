/**
 * \file            stm32_io_test.c
 * \brief           Real STM32 provider register model success and fault checks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_provider.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

nx_stm32_system_t g_nx_stm32_system;
GPIO_TypeDef g_nx_stm32_gpioa_model;
static nx_time_us_t s_time;
static uint32_t s_mask;
static unsigned s_fault;
static unsigned s_polls;
static bool s_isr;
static unsigned s_nvic_disable;
static unsigned s_nvic_clear;
static int s_nvic_irq;
static unsigned s_delayed_clock_read;
static unsigned s_delayed_barrier;
static uint8_t s_flash_memory[1048576] __attribute__((aligned(4)));

/** \brief Expose the ordinary model readback, preserving the real pointer. */
uint32_t nx_stm32_model_gpio_clock_read(const volatile uint32_t* reg) {
    return *reg;
}

/** \brief Record actual shared-vector disable decisions in the host model. */
void nx_stm32_model_nvic_disable(int irq) {
    ++s_nvic_disable;
    s_nvic_irq = irq;
}

/** \brief Record vector pending acknowledgement without invoking host MMIO. */
void nx_stm32_model_nvic_clear(int irq) {
    ++s_nvic_clear;
    assert(irq == s_nvic_irq);
}

/** \brief Model one finite microsecond clock step per hardware poll. */
nx_time_us_t nx_time_now_us(void) {
    if (s_delayed_clock_read != 0U && --s_delayed_clock_read == 0U) {
        s_time += 1000U;
    }
    return s_time;
}
/** \brief Model exact incoming mask restoration. */
nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t old = {s_mask};
    s_mask = 1U;
    return old;
}
/** \brief Restore the caller's mask. */
void nx_arch_irq_restore(nx_arch_irq_state_t old) {
    s_mask = old.value;
}
/** \brief Report the configured exception context. */
bool nx_arch_in_isr(void) {
    return s_isr;
}
/** \brief Report the configured exception mask. */
bool nx_arch_irq_is_masked(void) {
    return s_mask != 0U;
}
/** \brief Host model barriers do not establish physical device timing. */
void nx_arch_dsb(void) {
    if (s_delayed_barrier != 0U && --s_delayed_barrier == 0U) {
        s_time += 1000U;
    }
}

/** \brief Host barrier placeholder, not hardware cache qualification. */
void nx_arch_isb(void) {
}

/** \brief Progress peripheral flags or keep the selected physical fault active.
 */
void nx_stm32_model_io_poll(unsigned kind, void* raw) {
    ++s_time;
    ++s_polls;
    if (kind == 1U) {
        nx_stm32_spi_state_t* port = raw;
        if (s_fault == 0U) {
            port->registers->SR = SPI_SR_TXE | SPI_SR_RXNE;
        } else if (s_fault == 2U) {
            port->registers->SR = SPI_SR_MODF;
        } else {
            port->registers->SR = SPI_SR_TXE;
        }
    } else if (kind == 2U) {
        nx_stm32_i2c_state_t* port = raw;
        if (s_fault == 3U) {
            port->registers->SR1 = I2C_SR1_AF;
        } else if (s_fault == 4U) {
            port->registers->SR1 = I2C_SR1_ARLO;
        } else if (s_fault == 5U) {
            port->registers->SR1 = 0U;
        } else {
            port->registers->SR1 = I2C_SR1_SB | I2C_SR1_ADDR | I2C_SR1_TXE |
                                   I2C_SR1_BTF | I2C_SR1_RXNE;
        }
        if (s_fault != 6U) {
            port->registers->CR1 &= ~(uint32_t)I2C_CR1_STOP;
        }
    } else if (kind == 3U) {
        nx_stm32_flash_state_t* port = raw;
        port->registers->CR &= ~(uint32_t)FLASH_CR_LOCK;
        if (s_fault == 7U) {
            port->registers->SR = FLASH_SR_WRPERR;
        } else {
            port->registers->SR = 0U;
            if ((port->registers->CR & FLASH_CR_STRT) != 0U) {
                size_t index = (port->registers->CR & FLASH_CR_SNB) >> 3U;
                const nx_flash_sector_t* sector =
                    &port->geometry->sectors[index];
                memset((void*)(port->memory + sector->offset), 0xFF,
                       sector->size);
                port->registers->CR &= ~(uint32_t)FLASH_CR_STRT;
            }
        }
    } else if (kind == 4U) {
        nx_stm32_watchdog_state_t* port = raw;
        if (s_fault != 8U) {
            port->rcc->CSR |= RCC_CSR_LSIRDY;
        }
        if (s_fault != 9U) {
            port->registers->SR = 0U;
        }
    } else if (kind == 5U) {
        nx_stm32_adc_state_t* port = raw;
        if ((port->registers->CR2 & ADC_CR2_SWSTART) != 0U) {
            if (s_fault == 10U) {
                port->registers->SR = 0U;
            } else if (s_fault == 11U) {
                port->registers->SR = ADC_SR_OVR;
            } else {
                port->registers->SR = ADC_SR_EOC;
                port->registers->DR = 2048U + port->registers->SQR3;
            }
        }
    }
}

/** \brief Verify authorization, atomic batch encoding and masked snapshots. */
static void gpio_test(void) {
    RCC_TypeDef rcc = {0};
    RCC_TypeDef* previous_rcc = g_nx_stm32_system.rcc;
    g_nx_stm32_system.rcc = &rcc;
    assert(nx_stm32_gpio_clock_enable(4U) == NX_SUCCESS);
    assert((rcc.AHB1ENR & (1U << 4U)) != 0U);
    assert(nx_stm32_gpio_clock_enable(9U) == NX_ERROR_INVALID);
    g_nx_stm32_gpioa_model.MODER = 3U << 26U;
    assert(nx_stm32_uart1_pins_prepare() == NX_SUCCESS);
    assert((rcc.APB2ENR & RCC_APB2ENR_USART1EN) != 0U);
    assert((g_nx_stm32_gpioa_model.MODER & (3U << 26U)) == (3U << 26U));
    assert((g_nx_stm32_gpioa_model.AFR[1] & 0xFF0U) == 0x770U);
    GPIO_TypeDef registers = {0};
    nx_stm32_gpio_state_t port = {
        .registers = &registers, .mask = 0x18U, .output = true};
    const nx_gpio_port_t port_api = {&nx_stm32_gpio_ops, &port};
    (void)port_api;
    assert(nx_stm32_gpio_initialize(&port, 8U) == NX_SUCCESS);
    assert(registers.BSRR == (8U | (16U << 16U)));
    assert(nx_gpio_port_write(&port_api, 8U, 16U) == NX_SUCCESS);
    uint32_t before = registers.BSRR;
    assert(nx_gpio_port_write(&port_api, 8U, 8U) == NX_ERROR_INVALID);
    assert(nx_gpio_port_write(&port_api, 1U, 0U) == NX_ERROR_PERMISSION);
    assert(registers.BSRR == before);
    registers.IDR = 0xFFFFU;
    uint32_t value = 0U;
    assert(nx_gpio_port_read(&port_api, &value) == NX_SUCCESS &&
           value == 0x18U);
    registers.ODR = 8U;
    assert(nx_gpio_port_toggle(&port_api, 0x18U) == NX_SUCCESS);
    assert(registers.BSRR == (16U | (8U << 16U)));
    assert(nx_stm32_gpio_stop(&port, 8U) == NX_SUCCESS);
    assert(nx_gpio_port_write(&port_api, 8U, 0U) == NX_ERROR_INVALID);
    registers.MODER = 0x5A5A5A5AU;
    registers.OTYPER = 0xA5A5U;
    registers.OSPEEDR = 0xA5A5A5A5U;
    registers.PUPDR = 0x55555555U;
    registers.AFR[0] = 0x12345678U;
    registers.ODR = 1U << 3U;
    uint16_t saved = nx_stm32_pin_capture(&registers, 3U);
    uint32_t mode = registers.MODER;
    uint32_t speed = registers.OSPEEDR;
    uint32_t pull = registers.PUPDR;
    uint32_t type = registers.OTYPER;
    uint32_t af = registers.AFR[0];
    assert(nx_stm32_pin_configure(&registers, 3U, 2U, 7U, 0U, false) ==
           NX_SUCCESS);
    nx_stm32_pin_restore(&registers, 3U, saved);
    assert(registers.MODER == mode && registers.OSPEEDR == speed &&
           registers.PUPDR == pull && registers.OTYPER == type &&
           registers.AFR[0] == af && registers.BSRR == (1U << 3U));
    /* The global model borrows this local RCC only inside this fixture scope.
     */
    g_nx_stm32_system.rcc = previous_rcc;
}

/** \brief Drive one RX observation under simulated interrupt context. */
static void rx_byte(nx_stm32_uart_state_t* port, uint8_t byte, uint32_t error) {
    port->registers->SR = USART_SR_RXNE | error;
    port->registers->DR = byte;
    s_isr = true;
    nx_stm32_uart_irq(port);
    s_isr = false;
    port->registers->SR = 0U;
}

/** \brief Verify IRQ/TC ownership, cancel/timeout drain, loss and late IRQ. */
static void uart_test(void) {
    USART_TypeDef registers = {0};
    nx_uart_rx_event_t storage[2];
    nx_stm32_uart_state_t port = {.registers = &registers,
                                  .baud = 115200U,
                                  .profile = NX_UART_RX_EVENTS,
                                  .rx_storage = storage,
                                  .rx_capacity = 2U,
                                  .irq = USART1_IRQn};
    const nx_uart_port_t port_api = {&nx_stm32_uart_ops, &port};
    (void)port_api;
    assert(nx_stm32_uart_initialize(&port, 84000000U) == NX_SUCCESS);
    const uint8_t data[] = {0x11U, 0x22U};
    nx_uart_tx_request_t request;
    nx_request_initialize(&request.base);
    assert(nx_uart_tx_prepare(&request, data, sizeof(data), 1000U) ==
           NX_SUCCESS);
    request.base.deadline = s_time;
    assert(nx_uart_port_submit(&port_api, &request) == NX_ERROR_TIMEOUT);
    assert(nx_request_state(&request.base) == NX_REQUEST_READY &&
           port.active == NULL);
    request.base.deadline = 1000U;
    assert(nx_uart_port_submit(&port_api, &request) == NX_SUCCESS);
    assert(nx_uart_port_submit(&port_api, &request) == NX_ERROR_BUSY);
    registers.SR = USART_SR_TXE;
    nx_stm32_uart_irq(&port);
    assert(registers.DR == 0x11U && port.tx_position == 1U);
    registers.SR = USART_SR_TXE | USART_SR_TC;
    nx_stm32_uart_irq(&port);
    assert(!port.tx_complete);
    assert(registers.DR == 0x22U && port.tx_position == 2U);
    nx_uart_port_service(&port_api);
    assert(nx_request_state(&request.base) == NX_REQUEST_ACTIVE);
    registers.SR = USART_SR_TC;
    nx_stm32_uart_irq(&port);
    nx_uart_port_service(&port_api);
    assert(nx_request_state(&request.base) == NX_REQUEST_SETTLED);
    assert(request.base.result == NX_SUCCESS && request.base.transferred == 2U);
    nx_stm32_uart_irq(&port);
    assert(port.active == NULL);
    assert(nx_uart_tx_prepare(&request, data, sizeof(data), 1000U) ==
           NX_SUCCESS);
    assert(nx_uart_port_submit(&port_api, &request) == NX_SUCCESS);
    registers.SR = USART_SR_TXE;
    nx_stm32_uart_irq(&port);
    assert(nx_uart_port_cancel(&port_api, &request) == NX_SUCCESS);
    nx_uart_port_service(&port_api);
    assert(nx_request_state(&request.base) == NX_REQUEST_DRAINING);
    registers.SR = USART_SR_TC;
    nx_stm32_uart_irq(&port);
    nx_uart_port_service(&port_api);
    assert(request.base.result == NX_ERROR_CANCELLED);
    assert(request.base.transferred == 1U);
    rx_byte(&port, 1U, 0U);
    rx_byte(&port, 2U, USART_SR_ORE);
    rx_byte(&port, 3U, 0U);
    nx_uart_rx_event_t output[3];
    size_t count = 0U;
    assert(nx_uart_port_read_events(&port_api, output, 3U, &count) ==
           NX_SUCCESS);
    assert(count == 3U && output[0].byte == 1U && output[1].byte == 2U);
    assert((output[1].flags & NX_UART_EVENT_OVERRUN) != 0U);
    assert(output[2].flags == (NX_UART_EVENT_LOSS | NX_UART_EVENT_NO_BYTE));
    rx_byte(&port, 4U, 0U);
    rx_byte(&port, 5U, 0U);
    rx_byte(&port, 6U, 0U);
    assert(nx_uart_port_read_events(&port_api, output, 1U, &count) ==
           NX_SUCCESS);
    assert(count == 1U && output[0].byte == 4U);
    rx_byte(&port, 7U, 0U);
    assert(nx_uart_port_read_events(&port_api, output, 3U, &count) ==
           NX_SUCCESS);
    assert(count == 2U && output[0].byte == 5U &&
           output[1].flags == (NX_UART_EVENT_LOSS | NX_UART_EVENT_NO_BYTE));
    rx_byte(&port, 8U, 0U);
    assert(nx_uart_port_read_events(&port_api, output, 3U, &count) ==
           NX_SUCCESS);
    assert(count == 1U && output[0].byte == 8U);
    registers.SR = USART_SR_PE;
    registers.DR = 0xFFU;
    nx_stm32_uart_irq(&port);
    registers.SR = 0U;
    assert(nx_uart_port_read_events(&port_api, output, 3U, &count) ==
           NX_SUCCESS);
    assert(count == 1U && (output[0].flags & NX_UART_EVENT_NO_BYTE) != 0U);

    assert(nx_uart_port_read_bytes(&port_api, (uint8_t*)output, 1U, &count) ==
           NX_ERROR_UNSUPPORTED);
    s_time = 1000U;
    assert(nx_uart_tx_prepare(&request, data, sizeof(data), 1001U) ==
           NX_SUCCESS);
    assert(nx_uart_port_submit(&port_api, &request) == NX_SUCCESS);
    registers.SR = USART_SR_TXE;
    nx_stm32_uart_irq(&port);
    s_time = 1002U;
    nx_uart_port_service(&port_api);
    assert(nx_request_state(&request.base) == NX_REQUEST_DRAINING);
    s_time += 1000U;
    nx_uart_port_service(&port_api);
    assert(nx_request_state(&request.base) == NX_REQUEST_QUARANTINED);
    assert(nx_uart_port_stop(&port_api) == NX_ERROR_BUSY);
    registers.SR = USART_SR_TC;
    nx_stm32_uart_irq(&port);
    nx_uart_port_service(&port_api);
    assert(request.base.result == NX_ERROR_TIMEOUT);
    assert(nx_uart_port_stop(&port_api) == NX_SUCCESS);
    nx_stm32_uart_irq(&port);
    assert(port.active == NULL);
    assert(nx_stm32_uart_initialize(&port, 84000000U) == NX_SUCCESS);
    s_time = 4000U;
    assert(nx_uart_tx_prepare(&request, data, 1U, 4005U) == NX_SUCCESS);
    assert(nx_uart_port_submit(&port_api, &request) == NX_SUCCESS);
    registers.SR = USART_SR_TXE;
    nx_stm32_uart_irq(&port);
    s_time = 4006U;
    registers.SR = USART_SR_TC;
    nx_stm32_uart_irq(&port);
    nx_uart_port_service(&port_api);
    assert(request.base.result == NX_ERROR_TIMEOUT);
    s_time = 5000U;
    assert(nx_uart_tx_prepare(&request, data, 1U, 5005U) == NX_SUCCESS);
    assert(nx_uart_port_submit(&port_api, &request) == NX_SUCCESS);
    registers.SR = USART_SR_TXE;
    nx_stm32_uart_irq(&port);
    s_time = 5004U;
    registers.SR = USART_SR_TC;
    nx_stm32_uart_irq(&port);
    assert(nx_uart_port_cancel(&port_api, &request) == NX_SUCCESS);
    s_time = 5100U;
    nx_uart_port_service(&port_api);
    assert(request.base.result == NX_SUCCESS);
    assert(nx_uart_port_stop(&port_api) == NX_SUCCESS);
    uint8_t byte_storage[2];
    port.profile = NX_UART_RX_BYTES;
    port.rx_storage = byte_storage;
    assert(nx_stm32_uart_initialize(&port, 84000000U) == NX_SUCCESS);
    rx_byte(&port, 0x44U, 0U);
    rx_byte(&port, 0x55U, USART_SR_PE);
    rx_byte(&port, 0x66U, 0U);
    uint8_t bytes[2];
    assert(nx_uart_port_read_bytes(&port_api, bytes, 2U, &count) ==
           NX_ERROR_OVERFLOW);
    assert(count == 2U && bytes[0] == 0x44U && bytes[1] == 0x55U);
    assert(nx_uart_port_read_bytes(&port_api, bytes, 2U, &count) ==
           NX_ERROR_EMPTY);
    registers.SR = USART_SR_FE;
    registers.DR = 0xEEU;
    nx_stm32_uart_irq(&port);
    registers.SR = 0U;
    assert(nx_uart_port_read_bytes(&port_api, bytes, 2U, &count) ==
           NX_ERROR_OVERFLOW);
    assert(count == 0U);

    s_isr = true;
    assert(nx_uart_port_stop(&port_api) == NX_ERROR_CONTEXT);
    s_isr = false;
    assert(nx_uart_port_stop(&port_api) == NX_SUCCESS);
}

/** \brief Verify complete CS transfer, reset abort and admission checks. */
static void spi_test(void) {
    SPI_TypeDef registers = {0};
    RCC_TypeDef rcc = {0};
    GPIO_TypeDef gpio = {0};
    nx_stm32_gpio_state_t cs = {.registers = &gpio, .mask = 1U, .output = true};
    const nx_gpio_port_t cs_api = {&nx_stm32_gpio_ops, &cs};
    (void)cs_api;
    assert(nx_stm32_gpio_initialize(&cs, 1U) == NX_SUCCESS);
    nx_stm32_spi_state_t port = {
        .registers = &registers, .rcc = &rcc, .clock_hz = 84000000U};
    const nx_spi_port_t port_api = {&nx_stm32_spi_ops, &port};
    (void)port_api;
    nx_stm32_spi_endpoint_state_t endpoint = {.port = &port,
                                              .cs = &cs,
                                              .cs_mask = 1U,
                                              .frequency_hz = 1000000U,
                                              .mode = 0U};
    const nx_spi_endpoint_t endpoint_api = {&nx_stm32_spi_endpoint_ops,
                                            &endpoint};
    (void)endpoint_api;
    const uint8_t tx[] = {1U, 2U, 3U};
    uint8_t rx[3] = {0};
    size_t count = 99U;
    s_fault = 0U;
    assert(nx_spi_endpoint_transfer(&endpoint_api, tx, rx, 3U, s_time + 100U,
                                    &count) == NX_SUCCESS);
    assert(count == 3U && memcmp(tx, rx, 3U) == 0 && gpio.BSRR == 1U);
    assert((registers.CR1 & SPI_CR1_SPE) == 0U);
    s_delayed_clock_read = 5U;
    assert(nx_spi_endpoint_transfer(&endpoint_api, tx, rx, 1U, s_time + 100U,
                                    &count) == NX_ERROR_TIMEOUT);
    assert(count == 1U && !port.active && !port.fault && gpio.BSRR == 1U);
    s_fault = 1U;
    assert(nx_spi_endpoint_transfer(&endpoint_api, tx, rx, 3U, s_time + 4U,
                                    &count) == NX_ERROR_TIMEOUT);
    assert(count == 0U && port.fault && !port.active && gpio.BSRR == 1U);
    assert(nx_spi_endpoint_transfer(&endpoint_api, tx, rx, 3U, s_time + 4U,
                                    &count) == NX_ERROR_STATE);
    registers.SR = SPI_SR_BSY;
    assert(nx_spi_port_recover(&port_api) == NX_ERROR_BUSY);
    assert(port.fault);
    registers.SR = 0U;
    port.active = true;
    assert(nx_spi_port_recover(&port_api) == NX_ERROR_BUSY);
    port.active = false;
    s_isr = true;
    assert(nx_spi_port_recover(&port_api) == NX_ERROR_CONTEXT);
    s_isr = false;
    assert(nx_spi_port_recover(&port_api) == NX_SUCCESS);
    assert(!port.fault);
    s_fault = 2U;
    assert(nx_spi_endpoint_transfer(&endpoint_api, tx, rx, 3U, s_time + 4U,
                                    &count) == NX_ERROR_IO);
}

/** \brief Verify repeated START profiles, explicit NACK/arbitration and
 * recovery. */
static void i2c_test(void) {
    I2C_TypeDef registers = {0};
    nx_stm32_i2c_state_t port = {
        .registers = &registers, .peripheral_mhz = 42U, .rate_hz = 100000U};
    const nx_i2c_port_t port_api = {&nx_stm32_i2c_ops, &port};
    (void)port_api;
    nx_stm32_i2c_endpoint_state_t endpoint = {.port = &port, .address = 0x50U};
    const nx_i2c_endpoint_t endpoint_api = {&nx_stm32_i2c_endpoint_ops,
                                            &endpoint};
    (void)endpoint_api;
    uint8_t command = 0x10U;
    uint8_t read[5] = {0};
    nx_i2c_message_t messages[] = {{&command, 1U, false}, {read, 1U, true}};
    size_t transferred = 0U;
    s_fault = 0U;
    assert(nx_stm32_i2c_initialize(&port) == NX_SUCCESS);
    for (size_t length = 1U; length <= 5U; ++length) {
        messages[1].length = length;
        assert(nx_i2c_endpoint_transaction(&endpoint_api, messages, 2U,
                                           s_time + 100U,
                                           &transferred) == NX_SUCCESS);
        assert(transferred == length + 1U && !port.active);
    }
    for (unsigned fault = 3U; fault <= 5U; ++fault) {
        s_fault = fault;
        nx_result_t result = nx_i2c_endpoint_transaction(
            &endpoint_api, messages, 2U, s_time + 8U, &transferred);
        assert(result == (fault == 3U   ? NX_ERROR_NACK
                          : fault == 4U ? NX_ERROR_ARBITRATION
                                        : NX_ERROR_TIMEOUT));
        assert(!port.active);
    }
    s_fault = 6U;
    assert(nx_i2c_endpoint_transaction(&endpoint_api, messages, 2U,
                                       s_time + 100U,
                                       &transferred) == NX_ERROR_IO);
    assert(port.fault && !port.active);
    registers.SR2 = I2C_SR2_BUSY;
    s_fault = 0U;
    assert(nx_i2c_port_recover(&port_api) == NX_ERROR_IO);
    registers.SR2 = 0U;
    assert(nx_i2c_port_recover(&port_api) == NX_SUCCESS);
    uint32_t enabled = registers.CR1;
    s_isr = true;
    assert(nx_i2c_port_recover(&port_api) == NX_ERROR_CONTEXT);
    s_isr = false;
    s_mask = 1U;
    assert(nx_i2c_port_recover(&port_api) == NX_ERROR_CONTEXT);
    s_mask = 0U;
    assert(registers.CR1 == enabled);
    port.initialized = false;
    assert(nx_i2c_port_recover(&port_api) == NX_ERROR_STATE);
    assert(registers.CR1 == enabled);
}

/** \brief Verify exact density, subtraction bounds, pulses and immutable
 * regions. */
static void flash_test(void) {
    FLASH_TypeDef registers = {0};
    nx_stm32_flash_state_t port = {.registers = &registers,
                                   .geometry = &g_nx_stm32_flash_ve,
                                   .memory = s_flash_memory,
                                   .supply_mv = 3300U};
    const nx_flash_port_t port_api = {&nx_stm32_flash_ops, &port};
    (void)port_api;
    memset(s_flash_memory, 0xFF, sizeof(s_flash_memory));
    assert(nx_flash_port_geometry(&port_api)->sector_count == 8U);
    uint8_t buffer[8] = {0};
    assert(nx_flash_port_read(&port_api, UINT32_MAX, buffer, sizeof(buffer)) ==
           NX_ERROR_INVALID);
    uint32_t word = 0x11223344U;
    s_fault = 0U;
    assert(nx_flash_port_program(&port_api, 4U, &word, 4U, s_time + 100U) ==
           NX_SUCCESS);
    assert(*(uint32_t*)(s_flash_memory + 4U) == word && !port.active);
    assert((registers.CR & FLASH_CR_LOCK) != 0U);
    assert(nx_flash_port_program(&port_api, 5U, &word, 4U, s_time + 100U) ==
           NX_ERROR_INVALID);
    nx_flash_region_t region = {
        .port = &port_api, .offset = 0U, .size = 16384U, .writable = false};
    assert(nx_flash_region_program(&region, 4U, &word, 4U, s_time + 100U) ==
           NX_ERROR_PERMISSION);
    assert(nx_flash_port_erase(&port_api, 0U, 16383U, s_time + 100U) ==
           NX_ERROR_INVALID);
    assert(nx_flash_port_erase(&port_api, 0U, 16384U, s_time + 100U) ==
           NX_SUCCESS);
    assert(s_flash_memory[4] == 0xFFU);
    s_delayed_barrier = 2U;
    assert(nx_flash_port_program(&port_api, 4U, &word, 4U, s_time + 100U) ==
           NX_ERROR_TIMEOUT);
    assert(*(uint32_t*)(s_flash_memory + 4U) == word && !port.active &&
           (registers.CR & FLASH_CR_LOCK) != 0U);
    s_delayed_barrier = 1U;
    assert(nx_flash_port_erase(&port_api, 0U, 16384U, s_time + 100U) ==
           NX_ERROR_TIMEOUT);
    assert(s_flash_memory[4] == 0xFFU && !port.active &&
           (registers.CR & FLASH_CR_LOCK) != 0U);
    s_fault = 7U;
    assert(nx_flash_port_program(&port_api, 4U, &word, 4U, s_time + 100U) ==
           NX_ERROR_IO);
    assert(!port.active && (registers.CR & FLASH_CR_LOCK) != 0U);
    port.geometry = &g_nx_stm32_flash_zg;
    assert(nx_flash_port_geometry(&port_api)->sector_count == 12U);
    assert(nx_flash_port_geometry(&port_api)->size == 1048576U);
}

/** \brief Verify pre-enable failure and irreversible post-enable failure. */
static void watchdog_test(void) {
    IWDG_TypeDef registers = {0};
    RCC_TypeDef rcc = {0};
    DBGMCU_TypeDef debug = {0};
    FLASH_TypeDef flash = {.OPTCR = FLASH_OPTCR_WDG_SW};
    nx_stm32_watchdog_state_t port = {.registers = &registers,
                                      .rcc = &rcc,
                                      .debug = &debug,
                                      .flash = &flash,
                                      .poll_limit = 4U};
    const nx_watchdog_port_t port_api = {&nx_stm32_watchdog_ops, &port};
    (void)port_api;
    nx_watchdog_state_t state;
    s_fault = 8U;
    assert(nx_watchdog_port_enable(&port_api, 1000000U, false, &state) ==
           NX_ERROR_IO);
    assert(!state.enabled);
    s_fault = 9U;
    registers.SR = 1U;
    assert(nx_watchdog_port_enable(&port_api, 1000000U, true, &state) ==
           NX_ERROR_IO);
    assert(state.enabled && state.irreversible && state.debug_freeze);
    assert(nx_watchdog_port_enable(&port_api, 1000000U, false, &state) ==
           NX_ERROR_STATE);
    assert(nx_watchdog_port_feed(&port_api) == NX_SUCCESS &&
           registers.KR == 0xAAAAU);
    assert(state.minimum_timeout_us < 1000000U &&
           state.maximum_timeout_us > 1000000U);
    g_nx_stm32_system.reset_cause = RCC_CSR_PORRSTF | RCC_CSR_IWDGRSTF;
    assert(nx_reset_cause() == (NX_RESET_POWER_ON | NX_RESET_IWDG));
}

/** \brief Verify shared-vector isolation, overflow and stop-before-reclamation.
 */
static void exti_test(void) {
    EXTI_TypeDef registers = {0};
    GPIO_TypeDef gpio = {0};
    SYSCFG_TypeDef mux = {0};
    nx_exti_event_t storage[1];
    nx_stm32_exti_state_t port = {.registers = &registers,
                                  .gpio = &gpio,
                                  .storage = storage,
                                  .capacity = 1U,
                                  .line = 5U,
                                  .edge = NX_EXTI_BOTH};
    const nx_exti_port_t port_api = {&nx_stm32_exti_ops, &port};
    (void)port_api;
    assert(nx_stm32_exti_initialize(&port, &mux, 0U) == NX_SUCCESS);
    nx_stm32_exti_state_t duplicate = port;
    const nx_exti_port_t duplicate_api = {&nx_stm32_exti_ops, &duplicate};
    (void)duplicate_api;
    duplicate.initialized = false;
    assert(nx_stm32_exti_initialize(&duplicate, &mux, 1U) == NX_ERROR_BUSY);
    nx_stm32_exti_state_t* ports[] = {&port};
    registers.IMR |= 1U << 6U;
    registers.PR = (1U << 5U) | (1U << 6U);
    gpio.IDR = 1U << 5U;
    nx_stm32_exti_dispatch(ports, 1U, 0x3E0U);
    assert((registers.PR & (1U << 6U)) != 0U);
    registers.PR |= 1U << 5U;
    nx_stm32_exti_dispatch(ports, 1U, 0x3E0U);
    nx_exti_event_t output[2];
    size_t count = 0U;
    assert(nx_exti_port_read(&port_api, output, 2U, &count) == NX_SUCCESS);
    assert(count == 2U && output[0].edge == NX_EXTI_RISING);
    assert(output[1].flags == NX_EXTI_EVENT_LOSS);
    s_nvic_disable = 0U;
    s_nvic_clear = 0U;
    assert(nx_exti_port_stop(&port_api) == NX_SUCCESS);
    assert(s_nvic_disable == 0U && s_nvic_clear == 0U);
    assert((registers.IMR & (1U << 6U)) != 0U);
    registers.PR |= 1U << 5U;
    nx_stm32_exti_dispatch(ports, 1U, 0x3E0U);
    assert(port.count == 0U);
    duplicate.line = 6U;
    duplicate.initialized = true;
    assert(nx_exti_port_stop(&duplicate_api) == NX_SUCCESS);
    assert(s_nvic_disable == 1U && s_nvic_clear == 1U &&
           s_nvic_irq == (int)EXTI9_5_IRQn);
}

/** \brief Verify zero/full duty, fixed shared period, idle pin and ADC faults.
 */
static void timer_adc_test(void) {
    TIM_TypeDef timer = {0};
    GPIO_TypeDef gpio = {0};
    nx_stm32_gpio_state_t idle = {
        .registers = &gpio, .mask = 64U, .output = true};
    const nx_gpio_port_t idle_api = {&nx_stm32_gpio_ops, &idle};
    (void)idle_api;
    assert(nx_stm32_gpio_initialize(&idle, 0U) == NX_SUCCESS);
    nx_stm32_pwm_state_t pwm = {.registers = &timer,
                                .inactive_gpio = &idle,
                                .inactive_mask = 64U,
                                .channel = 1U,
                                .period_ticks = 1000U,
                                .tick_hz = 1000000U,
                                .prescaler = 83U};
    const nx_pwm_port_t pwm_api = {&nx_stm32_pwm_ops, &pwm};
    (void)pwm_api;
    assert(nx_stm32_pwm_initialize(&pwm) == NX_SUCCESS);
    assert(nx_pwm_port_set(&pwm_api, 1000U, 0U) == NX_SUCCESS &&
           timer.CCR1 == 0U);
    assert(nx_pwm_port_set(&pwm_api, 1000U, 1000U) == NX_SUCCESS &&
           timer.CCR1 == 1000U);
    assert(nx_pwm_port_set(&pwm_api, 999U, 500U) == NX_ERROR_STATE);
    assert(nx_pwm_port_start(&pwm_api) == NX_SUCCESS && pwm.running);
    assert(nx_pwm_port_stop(&pwm_api) == NX_SUCCESS && !pwm.running);
    assert((timer.CR1 & TIM_CR1_CEN) == 0U && gpio.BSRR == (64U << 16U));
    ADC_TypeDef adc = {0};
    ADC_Common_TypeDef common = {0};
    uint8_t channels[] = {0U, 1U};
    uint8_t sampling[] = {7U, 7U};
    nx_stm32_adc_state_t port = {.registers = &adc,
                                 .common = &common,
                                 .channels = channels,
                                 .sample_times = sampling,
                                 .channel_count = 2U,
                                 .reference_mv = 3300U};
    const nx_adc_port_t port_api = {&nx_stm32_adc_ops, &port};
    (void)port_api;
    assert(nx_stm32_adc_initialize(&port) == NX_SUCCESS);
    uint16_t samples[2] = {0};
    size_t count;
    s_fault = 0U;
    assert(nx_adc_port_sample(&port_api, samples, 2U, s_time + 100U, &count) ==
           NX_SUCCESS);
    assert(count == 2U && samples[0] == 2048U && samples[1] == 2049U);
    s_delayed_barrier = 1U;
    assert(nx_adc_port_sample(&port_api, samples, 2U, s_time + 100U, &count) ==
           NX_ERROR_TIMEOUT);
    assert(count == 2U && !port.active && adc.CR2 == 0U);
    samples[1] = 99U;
    s_fault = 10U;
    assert(nx_adc_port_sample(&port_api, samples, 2U, s_time + 5U, &count) ==
           NX_ERROR_TIMEOUT);
    assert(count == 0U && samples[1] == 99U && !port.active && adc.CR2 == 0U);
    s_fault = 11U;
    assert(nx_adc_port_sample(&port_api, samples, 2U, s_time + 20U, &count) ==
           NX_ERROR_IO);
    assert(count == 0U && adc.CR2 == 0U);
}

/** \brief Execute actual provider functions, never a stand-in API
 * implementation. */
int main(void) {
    gpio_test();
    /* A later constructor must reject unbound RCC, not use an expired fixture.
     */
    assert(nx_stm32_gpio_clock_enable(4U) == NX_ERROR_INVALID);
    uart_test();
    spi_test();
    i2c_test();
    flash_test();
    watchdog_test();
    exti_test();
    timer_adc_test();
    assert(s_mask == 0U && s_polls != 0U);
    puts("STM32 typed providers: 8 groups and real register faults passed");
    return 0;
}
