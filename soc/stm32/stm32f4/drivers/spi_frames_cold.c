/**
 * \file            spi_frames_cold.c
 * \brief           Exact SPI frame owner cold acquisition and restoration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
#include "stm32f407_spi_frames.h"

#ifndef NX_SPI_FRAMES_WRITE
#define NX_SPI_FRAMES_WRITE(address, value) (*(address) = (uint32_t)(value))
#endif
#ifndef NX_SPI_FRAMES_GPIO
#define NX_SPI_FRAMES_GPIO(port)                                               \
    ((GPIO_TypeDef*)(uintptr_t)(GPIOA_BASE + (uintptr_t)(port)*0x400U))
#endif
#ifndef NX_SPI_FRAMES_INSTANCE
#define NX_SPI_FRAMES_INSTANCE(irq)                                            \
    ((irq) == SPI1_IRQn ? SPI1 : (irq) == SPI2_IRQn ? SPI2 : SPI3)
#endif
#ifndef NX_SPI_FRAMES_RCC
#define NX_SPI_FRAMES_RCC() RCC
#endif
#ifndef NX_SPI_FRAMES_IRQ_PRIORITY_SET
#define NX_SPI_FRAMES_IRQ_PRIORITY_SET(irq, priority)                          \
    NVIC_SetPriority((irq), (priority))
#define NX_SPI_FRAMES_IRQ_PRIORITY(irq) NVIC_GetPriority(irq)
#endif

/** \brief Select only the actual peripheral gate, preserving every neighbour.
 */
static volatile uint32_t* apb(const nx_stm32_spi_frames_config_t* config) {
    return config->irq == SPI1_IRQn ? &config->rcc->APB2ENR
                                    : &config->rcc->APB1ENR;
}

/** \brief Gate/reset facts are independent of configured pin order. */
static uint32_t gate(const nx_stm32_spi_frames_config_t* config) {
    return config->irq == SPI1_IRQn   ? RCC_APB2ENR_SPI1EN
           : config->irq == SPI2_IRQn ? RCC_APB1ENR_SPI2EN
                                      : RCC_APB1ENR_SPI3EN;
}

/** \brief DS8626 AF facts for the reviewed SPI1, SPI2 and SPI3 primary pads.
 */
static bool pad(const nx_stm32_spi_frames_cold_config_t* config, size_t index,
                unsigned* bank, unsigned* pin) {
    const nx_stm32_spi_frames_config_t* engine = &config->engine;
    bool first = engine->irq == SPI1_IRQn;
    bool third = engine->irq == SPI3_IRQn;
    unsigned base = first ? 5U : third ? 10U : 13U;
    *bank = first ? 0U : third ? 2U : 1U;
    if (index == 0U) {
        *pin = base;
    } else if (index + 1U == config->pin_count) {
        *bank = third ? 0U : *bank;
        *pin = third ? 15U : base - 1U;
    } else if (engine->wire.lines == NX_SPI_FRAMES_TWO_LINE) {
        *pin = base + (unsigned)index;
    } else {
        bool miso = engine->wire.lines == NX_SPI_FRAMES_RECEIVE_ONLY
                        ? engine->wire.role == NX_SPI_FRAMES_MASTER
                        : engine->wire.role == NX_SPI_FRAMES_SLAVE;
        *pin = base + (miso ? 1U : 2U);
    }
    return true;
}

/** \brief Check complete pin identity before acquiring any register or pad. */
static bool binding_valid(const nx_stm32_spi_frames_cold_config_t* config) {
    if (config == NULL || config->pins == NULL ||
        config->engine.registers !=
            NX_SPI_FRAMES_INSTANCE(config->engine.irq) ||
        config->engine.rcc != NX_SPI_FRAMES_RCC() ||
        config->pin_count !=
            (config->engine.wire.lines == NX_SPI_FRAMES_TWO_LINE ? 4U : 3U))
        return false;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_spi_pin_config_t* pin = &config->pins[i];
        unsigned bank, number;
        (void)pad(config, i, &bank, &number);
        if (pin->snapshot == NULL ||
            pin->registers != NX_SPI_FRAMES_GPIO(bank) ||
            pin->port_index != bank || pin->pin != number ||
            pin->alternate != (config->engine.irq == SPI3_IRQn ? 6U : 5U))
            return false;
        for (size_t j = 0U; j < i; ++j)
            if (pin->snapshot == config->pins[j].snapshot)
                return false;
    }
    const nx_stm32_spi_pin_config_t* sck = &config->pins[0];
    const nx_stm32_spi_pin_config_t* nss =
        &config->pins[config->pin_count - 1U];
    return config->engine.sck_gpio == sck->registers &&
           config->engine.sck_mask == (1U << sck->pin) &&
           config->engine.nss_gpio == nss->registers &&
           config->engine.nss_mask == (1U << nss->pin);
}

/** \brief Compare complete extents without overflowing an end pointer. */
static bool intersects(const void* first, size_t first_bytes,
                       const void* second, size_t second_bytes) {
    uintptr_t left = (uintptr_t)first;
    uintptr_t right = (uintptr_t)second;
    return left <= right ? right - left < first_bytes
                         : left - right < second_bytes;
}

/** \brief Snapshot writes cannot corrupt their own later cleanup records. */
static bool storage_valid(const nx_stm32_spi_frames_state_t* state,
                          const nx_stm32_spi_frames_cold_config_t* config,
                          const nx_stm32_spi_frames_cold_t* receipt) {
    size_t pin_bytes = config->pin_count * sizeof(*config->pins);
    if (intersects(state, sizeof(*state), config, sizeof(*config)) ||
        intersects(state, sizeof(*state), receipt, sizeof(*receipt)) ||
        intersects(config, sizeof(*config), receipt, sizeof(*receipt)) ||
        intersects(config->pins, pin_bytes, state, sizeof(*state)) ||
        intersects(config->pins, pin_bytes, config, sizeof(*config)) ||
        intersects(config->pins, pin_bytes, receipt, sizeof(*receipt)))
        return false;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const void* snapshot = config->pins[i].snapshot;
        if ((uintptr_t)snapshot % _Alignof(uint16_t) != 0U ||
            sizeof(uint16_t) - 1U > UINTPTR_MAX - (uintptr_t)snapshot ||
            intersects(snapshot, sizeof(uint16_t), state, sizeof(*state)) ||
            intersects(snapshot, sizeof(uint16_t), config, sizeof(*config)) ||
            intersects(snapshot, sizeof(uint16_t), receipt, sizeof(*receipt)) ||
            intersects(snapshot, sizeof(uint16_t), config->pins, pin_bytes))
            return false;
        for (size_t j = 0U; j < i; ++j)
            if (intersects(snapshot, sizeof(uint16_t), config->pins[j].snapshot,
                           sizeof(uint16_t)))
                return false;
    }
    return true;
}

/** \brief Reject retained effects and malformed facts without a new write. */
nx_result_t
nx_stm32_spi_frames_admit(const nx_stm32_spi_frames_state_t* state,
                          const nx_stm32_spi_frames_cold_config_t* config,
                          const nx_stm32_spi_frames_cold_t* receipt) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (state == NULL || receipt == NULL || !binding_valid(config) ||
        !storage_valid(state, config, receipt))
        return NX_ERROR_INVALID;
    if (receipt->owned || receipt->registers_captured ||
        receipt->vector_captured || receipt->captured != 0U ||
        receipt->gpio_mask != 0U || receipt->cr1 != 0U || receipt->cr2 != 0U ||
        receipt->crcpr != 0U || receipt->apb != 0U ||
        receipt->gpio_clocks != 0U || receipt->priority != 0U)
        return NX_ERROR_BUSY;
    return nx_stm32_spi_frames_preflight(state, &config->engine);
}

/** \brief Disconnect electrical drive before restoring an old latch or mux. */
static bool disconnect(const nx_stm32_spi_pin_config_t* pin) {
    uint32_t mask = 3U << (pin->pin * 2U);
    NX_SPI_FRAMES_WRITE(&pin->registers->MODER, pin->registers->MODER | mask);
    nx_arch_dsb();
    return (pin->registers->MODER & mask) == mask;
}

/** \brief Restore original resources only after the owner proves quiet. */
nx_result_t
nx_stm32_spi_frames_close(nx_stm32_spi_frames_state_t* state,
                          const nx_stm32_spi_frames_cold_config_t* config,
                          nx_stm32_spi_frames_cold_t* receipt) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (state == NULL || receipt == NULL || !binding_valid(config))
        return NX_ERROR_INVALID;
    if (!receipt->owned)
        return state->config == NULL ? NX_SUCCESS : NX_ERROR_STATE;
    if (state->config != NULL && state->config != &config->engine)
        return NX_ERROR_STATE;
    if (state->config != NULL) {
        nx_result_t result = nx_stm32_spi_frames_ops.stop(state);
        if (result != NX_SUCCESS)
            return result;
    }
    if (receipt->registers_captured) {
        SPI_TypeDef* registers = config->engine.registers;
        if ((registers->CR1 & SPI_CR1_SPE) != 0U ||
            (registers->CR2 & (SPI_CR2_TXEIE | SPI_CR2_RXNEIE | SPI_CR2_ERRIE |
                               SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN)) != 0U)
            return NX_ERROR_BUSY;
        NX_SPI_FRAMES_WRITE(&registers->CR2, receipt->cr2);
        NX_SPI_FRAMES_WRITE(&registers->CRCPR, receipt->crcpr);
        NX_SPI_FRAMES_WRITE(&registers->CR1, receipt->cr1);
        nx_arch_dsb();
        if (registers->CR1 != receipt->cr1 || registers->CR2 != receipt->cr2 ||
            registers->CRCPR != receipt->crcpr)
            return NX_ERROR_IO;
        receipt->registers_captured = false;
    }
    while (receipt->captured != 0U) {
        const nx_stm32_spi_pin_config_t* pin =
            &config->pins[receipt->captured - 1U];
        if (!disconnect(pin))
            return NX_ERROR_IO;
        nx_stm32_pin_restore(pin->registers, pin->pin, *pin->snapshot);
        if (nx_stm32_pin_capture(pin->registers, pin->pin) != *pin->snapshot)
            return NX_ERROR_IO;
        --receipt->captured;
    }
    if (receipt->vector_captured) {
        NX_SPI_FRAMES_IRQ_PRIORITY_SET(config->engine.irq, receipt->priority);
        nx_arch_dsb();
        if (NX_SPI_FRAMES_IRQ_PRIORITY(config->engine.irq) != receipt->priority)
            return NX_ERROR_IO;
        receipt->vector_captured = false;
    }
    RCC_TypeDef* rcc = config->engine.rcc;
    NX_SPI_FRAMES_WRITE(&rcc->AHB1ENR, (rcc->AHB1ENR & ~receipt->gpio_mask) |
                                           receipt->gpio_clocks);
    nx_arch_dsb();
    if ((rcc->AHB1ENR & receipt->gpio_mask) != receipt->gpio_clocks)
        return NX_ERROR_IO;
    volatile uint32_t* clock = apb(&config->engine);
    uint32_t mask = gate(&config->engine);
    NX_SPI_FRAMES_WRITE(clock, (*clock & ~mask) | receipt->apb);
    nx_arch_dsb();
    if ((*clock & mask) != receipt->apb)
        return NX_ERROR_IO;
    uint64_t generation = state->generation;
    *state = (nx_stm32_spi_frames_state_t){0};
    state->generation = generation;
    *receipt = (nx_stm32_spi_frames_cold_t){0};
    return NX_SUCCESS;
}

/** \brief Apply the selected tuple only after pure complete admission. */
nx_result_t
nx_stm32_spi_frames_prepare(nx_stm32_spi_frames_state_t* state,
                            const nx_stm32_spi_frames_cold_config_t* config,
                            nx_stm32_spi_frames_cold_t* receipt) {
    nx_result_t result = nx_stm32_spi_frames_admit(state, config, receipt);
    if (result != NX_SUCCESS)
        return result;
    RCC_TypeDef* rcc = config->engine.rcc;
    if (!nx_stm32_clock_profile_matches(rcc, NULL))
        return NX_ERROR_STATE;
    volatile uint32_t* clock = apb(&config->engine);
    uint32_t mask = gate(&config->engine);
    receipt->apb = *clock & mask;
    receipt->owned = true;
    NX_SPI_FRAMES_WRITE(clock, *clock | mask);
    nx_arch_dsb();
    result = NX_ERROR_IO;
    if ((*clock & mask) == 0U)
        goto failed;
    SPI_TypeDef* registers = config->engine.registers;
    if ((registers->CR1 & SPI_CR1_SPE) != 0U ||
        (registers->CR2 & (SPI_CR2_TXEIE | SPI_CR2_RXNEIE | SPI_CR2_ERRIE |
                           SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN)) != 0U) {
        result = NX_ERROR_BUSY;
        goto failed;
    }
    receipt->cr1 = registers->CR1;
    receipt->cr2 = registers->CR2;
    receipt->crcpr = registers->CRCPR;
    receipt->registers_captured = true;
    receipt->priority = NX_SPI_FRAMES_IRQ_PRIORITY(config->engine.irq);
    receipt->vector_captured = true;
    for (size_t i = 0U; i < config->pin_count; ++i)
        receipt->gpio_mask |= 1U << config->pins[i].port_index;
    receipt->gpio_clocks = rcc->AHB1ENR & receipt->gpio_mask;
    NX_SPI_FRAMES_WRITE(&rcc->AHB1ENR, rcc->AHB1ENR | receipt->gpio_mask);
    nx_arch_dsb();
    if ((rcc->AHB1ENR & receipt->gpio_mask) != receipt->gpio_mask)
        goto failed;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_spi_pin_config_t* pin = &config->pins[i];
        *pin->snapshot = nx_stm32_pin_capture(pin->registers, pin->pin);
        receipt->captured = i + 1U;
        result = NX_ERROR_IO;
        if (!disconnect(pin))
            goto failed;
        result = nx_stm32_pin_configure(pin->registers, pin->pin, 2U,
                                        pin->alternate, 0U, false);
        uint32_t shift = pin->pin * 2U;
        if (result != NX_SUCCESS ||
            ((pin->registers->MODER >> shift) & 3U) != 2U ||
            ((pin->registers->OSPEEDR >> shift) & 3U) != 2U ||
            ((pin->registers->OTYPER >> pin->pin) & 1U) != 0U ||
            ((pin->registers->PUPDR >> shift) & 3U) != 0U ||
            ((pin->registers->AFR[pin->pin / 8U] >> ((pin->pin % 8U) * 4U)) &
             15U) != pin->alternate) {
            result = NX_ERROR_IO;
            goto failed;
        }
    }
    result = nx_stm32_spi_frames_initialize(state, &config->engine);
    if (result == NX_SUCCESS) {
        state->cold_config = config;
        state->cold_receipt = receipt;
        return NX_SUCCESS;
    }
failed:
    (void)nx_stm32_spi_frames_close(state, config, receipt);
    return result;
}
