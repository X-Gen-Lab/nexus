/**
 * \file            i2c_target.c
 * \brief           Exclusive finite F407 target and SMBus exchanges
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
#include "stm32f407_i2c_target.h"
#include "stm32f4xx_ll_i2c.h"

#ifndef NX_I2C_TARGET_READ
#define NX_I2C_TARGET_READ(p) (*(p))
#endif
#ifndef NX_I2C_TARGET_WRITE
#define NX_I2C_TARGET_WRITE(p, v) (*(p) = (uint32_t)(v))
#endif
#ifndef NX_I2C_TARGET_IRQ_ENABLE
#define NX_I2C_TARGET_IRQ_ENABLE(i)          NVIC_EnableIRQ(i)
#define NX_I2C_TARGET_IRQ_DISABLE(i)         NVIC_DisableIRQ(i)
#define NX_I2C_TARGET_IRQ_CLEAR(i)           NVIC_ClearPendingIRQ(i)
#define NX_I2C_TARGET_IRQ_PRIORITY_SET(i, p) NVIC_SetPriority(i, p)
#define NX_I2C_TARGET_IRQ_ENABLED(i)         NVIC_GetEnableIRQ(i)
#define NX_I2C_TARGET_IRQ_PENDING(i)         NVIC_GetPendingIRQ(i)
#define NX_I2C_TARGET_IRQ_ACTIVE(i)          NVIC_GetActive(i)
#define NX_I2C_TARGET_IRQ_PRIORITY(i)        NVIC_GetPriority(i)
#endif

/** \brief Establish nonwrapping metadata before typed access. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}

/** \brief Compare already validated spans without overflowing an end. */
static bool overlap(const void* a, size_t na, const void* b, size_t nb) {
    uintptr_t left = (uintptr_t)a;
    uintptr_t right = (uintptr_t)b;
    return na != 0U && nb != 0U &&
           (left <= right ? right - left < na : left - right < nb);
}

/** \brief Cold controls preserve all incoming CPU mask/context restrictions. */
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief Resolve only the three reviewed physical controllers. */
static unsigned controller(const nx_stm32_i2c_target_config_t* config) {
    return config->registers == I2C1   ? 0U
           : config->registers == I2C2 ? 1U
           : config->registers == I2C3 ? 2U
                                       : 3U;
}

/** \brief Gate and reset bits share the same F407 APB1 position. */
static uint32_t gate(const nx_stm32_i2c_target_config_t* config) {
    return RCC_APB1ENR_I2C1EN << controller(config);
}

/** \brief Event/error vectors belong to the same exclusive C state. */
static IRQn_Type vector(const nx_stm32_i2c_target_config_t* config,
                        unsigned index) {
    if (config->registers == I2C1)
        return index == 0U ? I2C1_EV_IRQn : I2C1_ER_IRQn;
    if (config->registers == I2C2)
        return index == 0U ? I2C2_EV_IRQn : I2C2_ER_IRQn;
    return index == 0U ? I2C3_EV_IRQn : I2C3_ER_IRQn;
}

/** \brief Check admitted AF4 pin storage without duplicating route tables.
 * \note SoC/package and Board route facts authorize SCL/SDA/optional SMBA.
 *       C retains bank identity, range and duplicate checks before effects.
 */
static bool route_valid(const nx_stm32_i2c_target_config_t* config) {
    for (size_t index = 0U; index < config->pin_count; ++index) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[index];
        if (!nx_stm32_platform_pin_valid(pin))
            return false;
        for (size_t previous = 0U; previous < index; ++previous) {
            if (pin->registers == config->pins[previous].registers &&
                pin->pin == config->pins[previous].pin)
                return false;
        }
    }
    return true;
}

/** \brief Reject inconsistent static facts before any live register access. */
static bool config_valid(const nx_stm32_i2c_target_config_t* config) {
    if (!span(config, sizeof(*config),
              _Alignof(nx_stm32_i2c_target_config_t)) ||
        controller(config) >= 3U || config->rcc != RCC ||
        config->pin_count != (config->alert ? 3U : 2U) ||
        !span(config->pins, config->pin_count * sizeof(*config->pins),
              _Alignof(nx_stm32_platform_pin_config_t)) ||
        config->priority > 15U || config->clock_hz != NEXUS_APB1_HZ ||
        config->clock_hz % 1000000U != 0U ||
        config->clock_hz <
            (config->maximum_hz > 100000U ? 4000000U : 2000000U) ||
        config->clock_hz > 50000000U || config->maximum_hz == 0U ||
        config->maximum_hz > 400000U ||
        (!config->ten_bit &&
         (config->own_address < 8U || config->own_address > 119U)) ||
        (config->ten_bit && config->own_address > 1023U) ||
        (config->second_address != 0U &&
         (config->second_address < 8U || config->second_address > 119U ||
          config->second_address == config->own_address)) ||
        (!config->smbus && (config->pec || config->alert)) ||
        (config->smbus && config->maximum_hz > 100000U) ||
        (config->pec &&
         (config->irq_latency_us == 0U ||
          (uint64_t)config->irq_latency_us * config->maximum_hz >= 6000000U)))
        return false;
    for (size_t index = 0U; index < config->pin_count; ++index) {
        if (!span(config->pins[index].snapshot, sizeof(uint16_t),
                  _Alignof(uint16_t)))
            return false;
        for (size_t previous = 0U; previous < index; ++previous)
            if (config->pins[index].snapshot == config->pins[previous].snapshot)
                return false;
    }
    return route_valid(config);
}

/** \brief Actual System/RCC and Flash constitute the only clock authority. */
static bool clock_live(const nx_stm32_i2c_target_config_t* config) {
    const nx_stm32_system_t* system = &g_nx_stm32_system;
    return system->started && system->rcc == config->rcc &&
           system->flash != NULL && system->timer_error == NX_SUCCESS &&
           system->core_hz == NEXUS_CORE_HZ && system->time_epoch != 0U &&
           nx_stm32_clock_profile_matches(config->rcc, system->flash);
}

/** \brief Caller storage never aliases System, MMIO or immutable methods. */
static bool hardware_overlap(const nx_stm32_i2c_target_config_t* config,
                             const void* pointer, size_t bytes) {
    if (overlap(pointer, bytes, (const void*)(uintptr_t)UINT32_C(0x40000000),
                UINT32_C(0x20000000)) ||
        overlap(pointer, bytes, (const void*)(uintptr_t)UINT32_C(0xE0000000),
                UINT32_C(0x10000000)) ||
        overlap(pointer, bytes, &g_nx_stm32_system,
                sizeof(g_nx_stm32_system)) ||
        overlap(pointer, bytes, &nx_stm32_i2c_target_ops,
                sizeof(nx_stm32_i2c_target_ops)) ||
        overlap(pointer, bytes, config->rcc, sizeof(*config->rcc)) ||
        overlap(pointer, bytes, I2C1, sizeof(*I2C1)) ||
        overlap(pointer, bytes, I2C2, sizeof(*I2C2)) ||
        overlap(pointer, bytes, I2C3, sizeof(*I2C3)))
        return true;
    for (size_t index = 0U; index < config->pin_count; ++index)
        if (overlap(pointer, bytes, config->pins[index].registers,
                    sizeof(GPIO_TypeDef)))
            return true;
    return false;
}

/** \brief Full storage separation protects immutable configuration/receipts. */
static bool cold_storage(const nx_stm32_i2c_target_state_t* state,
                         const nx_stm32_i2c_target_config_t* config,
                         const nx_stm32_i2c_target_cold_t* receipt) {
    if (!span(state, sizeof(*state), _Alignof(nx_stm32_i2c_target_state_t)) ||
        !span(receipt, sizeof(*receipt),
              _Alignof(nx_stm32_i2c_target_cold_t)) ||
        hardware_overlap(config, state, sizeof(*state)) ||
        hardware_overlap(config, receipt, sizeof(*receipt)) ||
        overlap(state, sizeof(*state), receipt, sizeof(*receipt)) ||
        overlap(state, sizeof(*state), config, sizeof(*config)) ||
        overlap(receipt, sizeof(*receipt), config, sizeof(*config)) ||
        overlap(state, sizeof(*state), config->pins,
                config->pin_count * sizeof(*config->pins)) ||
        overlap(receipt, sizeof(*receipt), config->pins,
                config->pin_count * sizeof(*config->pins)))
        return false;
    for (size_t index = 0U; index < config->pin_count; ++index) {
        const void* snapshot = config->pins[index].snapshot;
        if (hardware_overlap(config, snapshot, sizeof(uint16_t)) ||
            overlap(snapshot, sizeof(uint16_t), state, sizeof(*state)) ||
            overlap(snapshot, sizeof(uint16_t), receipt, sizeof(*receipt)) ||
            overlap(snapshot, sizeof(uint16_t), config, sizeof(*config)) ||
            overlap(snapshot, sizeof(uint16_t), config->pins,
                    config->pin_count * sizeof(*config->pins)))
            return false;
    }
    return true;
}

static uint32_t error_facts(uint32_t status);

/** \brief Prove sources/clock stopped without using any previous deadline. */
static bool reset_controller(nx_stm32_i2c_target_state_t* state) {
    const nx_stm32_i2c_target_config_t* config = state->config;
    I2C_TypeDef* regs = config->registers;
    uint32_t bit = gate(config);
    NX_I2C_TARGET_WRITE(&regs->CR2, config->clock_hz / 1000000U);
    if (state->active != NULL && state->terminal_latched) {
        /* Late data/ADDR/error facts after the completion boundary cannot be
         * reset out of existence and reported as success. No late DR read or
         * caller payload access is needed to preserve the failed exchange. */
        uint32_t late = NX_I2C_TARGET_READ(&regs->SR1);
        state->faults |= error_facts(late);
        if ((late & (I2C_SR1_RXNE | I2C_SR1_ADDR | I2C_SR1_SMBALERT)) != 0U)
            state->faults |= NX_I2C_TARGET_PROTOCOL_ERROR;
        if (state->terminal == NX_SUCCESS && state->faults != 0U) {
            /* Expected ARA arbitration can accompany a later success. */
            uint32_t invalid = state->faults;
            if (state->active->direction == NX_I2C_TARGET_ALERT_RESPONSE)
                invalid &= ~(uint32_t)NX_I2C_TARGET_ARBITRATION;
            if (invalid != 0U) {
                state->terminal = NX_ERROR_IO;
                state->faulted = true;
            }
        }
    }
    /* PE=0 may wait for the current communication. Reset is the explicit
     * SDK gap needed for bounded target abort; it releases only our outputs,
     * never claims another controller's bus has recovered. */
    NX_I2C_TARGET_WRITE(&config->rcc->APB1RSTR,
                        NX_I2C_TARGET_READ(&config->rcc->APB1RSTR) | bit);
    nx_arch_dsb();
    if ((NX_I2C_TARGET_READ(&config->rcc->APB1RSTR) & bit) == 0U)
        return false;
    NX_I2C_TARGET_WRITE(&config->rcc->APB1RSTR,
                        NX_I2C_TARGET_READ(&config->rcc->APB1RSTR) & ~bit);
    nx_arch_dsb();
    if ((NX_I2C_TARGET_READ(&config->rcc->APB1RSTR) & bit) != 0U ||
        (NX_I2C_TARGET_READ(&regs->CR1) &
         (I2C_CR1_PE | I2C_CR1_ACK | I2C_CR1_ALERT)) != 0U ||
        (NX_I2C_TARGET_READ(&regs->CR2) &
         (I2C_CR2_DMAEN | I2C_CR2_ITERREN | I2C_CR2_ITEVTEN |
          I2C_CR2_ITBUFEN)) != 0U)
        return false;
    return true;
}

/** \brief Thread joins both IRQ observers after proving the peripheral quiet.
 */
static bool withdraw(nx_stm32_i2c_target_state_t* state) {
    const nx_stm32_i2c_target_config_t* config = state->config;
    if (!reset_controller(state))
        return false;
    for (unsigned index = 0U; index < 2U; ++index) {
        IRQn_Type irq = vector(config, index);
        NX_I2C_TARGET_IRQ_DISABLE(irq);
        nx_arch_dsb();
        if (NX_I2C_TARGET_IRQ_ENABLED(irq) != 0U ||
            NX_I2C_TARGET_IRQ_ACTIVE(irq) != 0U)
            return false;
        NX_I2C_TARGET_IRQ_CLEAR(irq);
        nx_arch_dsb();
        if (NX_I2C_TARGET_IRQ_PENDING(irq) != 0U)
            return false;
    }
    return true;
}

/** \brief Register order is fixed C control, with no runtime script table. */
static void capture_registers(I2C_TypeDef* regs, uint32_t saved[6]) {
    saved[0] = NX_I2C_TARGET_READ(&regs->CR1);
    saved[1] = NX_I2C_TARGET_READ(&regs->CR2);
    saved[2] = NX_I2C_TARGET_READ(&regs->OAR1);
    saved[3] = NX_I2C_TARGET_READ(&regs->OAR2);
    saved[4] = NX_I2C_TARGET_READ(&regs->CCR);
    saved[5] = NX_I2C_TARGET_READ(&regs->TRISE);
}

/** \brief Restore only the admitted powered, disabled register tuple. */
static bool restore_registers(I2C_TypeDef* regs, const uint32_t saved[6]) {
    NX_I2C_TARGET_WRITE(&regs->OAR1, saved[2]);
    NX_I2C_TARGET_WRITE(&regs->OAR2, saved[3]);
    NX_I2C_TARGET_WRITE(&regs->CCR, saved[4]);
    NX_I2C_TARGET_WRITE(&regs->TRISE, saved[5]);
    NX_I2C_TARGET_WRITE(&regs->CR2, saved[1]);
    NX_I2C_TARGET_WRITE(&regs->CR1, saved[0]);
    nx_arch_dsb();
    uint32_t observed[6];
    capture_registers(regs, observed);
    for (unsigned index = 0U; index < 6U; ++index)
        if (observed[index] != saved[index])
            return false;
    return true;
}

/** \brief Reapply the reviewed address/stretch/PEC tuple after reset. */
static bool program_controller(const nx_stm32_i2c_target_config_t* config) {
    uint32_t mhz = config->clock_hz / 1000000U;
    bool fast = config->maximum_hz > 100000U;
    /* RM0090 27.6.8: CCR generates SCL only in controller mode. This owner
     * never generates START; a legal inactive divisor is independent of the
     * external target SCL ceiling and cannot overflow for a slow peer. */
    uint32_t ccr = 4U;
    uint32_t selected[6] = {
        I2C_CR1_PE | (config->smbus ? I2C_CR1_SMBUS : 0U) |
            (config->pec ? I2C_CR1_ENPEC : 0U),
        mhz,
        (1U << 14U) | (config->ten_bit ? I2C_OAR1_ADDMODE | config->own_address
                                       : (uint32_t)config->own_address << 1U),
        config->second_address == 0U
            ? 0U
            : ((uint32_t)config->second_address << 1U) | I2C_OAR2_ENDUAL,
        ccr | (fast ? I2C_CCR_FS : 0U),
        fast ? mhz * 3U / 10U + 1U : mhz + 1U};
    return restore_registers(config->registers, selected);
}

static nx_result_t stop_owner(void* opaque);

nx_result_t
nx_stm32_i2c_target_admit(const nx_stm32_i2c_target_state_t* state,
                          const nx_stm32_i2c_target_config_t* config,
                          const nx_stm32_i2c_target_cold_t* receipt) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || !cold_storage(state, config, receipt))
        return NX_ERROR_INVALID;
    if (config->ten_bit && (config->second_address != 0U || config->smbus))
        return NX_ERROR_UNSUPPORTED;
    if (state->config != NULL || state->active != NULL || state->initialized ||
        receipt->owned || receipt->owner != NULL)
        return NX_ERROR_BUSY;
    if (state->generation == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    /* The Platform invokes readonly admission before System.Start. An already
     * running System must match; first preparation checks the RUN clock again
     * after the Platform has acquired it. Foreign activity is checked in both
     * contexts before System is allowed to change clocks. */
    if (g_nx_stm32_system.started && !clock_live(config))
        return NX_ERROR_STATE;
    if ((NX_I2C_TARGET_READ(&config->rcc->APB1RSTR) & gate(config)) != 0U)
        return NX_ERROR_BUSY;
    for (unsigned index = 0U; index < 2U; ++index) {
        IRQn_Type irq = vector(config, index);
        if (NX_I2C_TARGET_IRQ_ENABLED(irq) != 0U ||
            NX_I2C_TARGET_IRQ_PENDING(irq) != 0U ||
            NX_I2C_TARGET_IRQ_ACTIVE(irq) != 0U)
            return NX_ERROR_BUSY;
    }
    if ((NX_I2C_TARGET_READ(&config->rcc->APB1ENR) & gate(config)) != 0U) {
        I2C_TypeDef* regs = config->registers;
        if (NX_I2C_TARGET_READ(&regs->CR1) != 0U ||
            (NX_I2C_TARGET_READ(&regs->CR2) & ~I2C_CR2_FREQ) != 0U ||
            NX_I2C_TARGET_READ(&regs->SR1) != 0U ||
            NX_I2C_TARGET_READ(&regs->SR2) != 0U)
            return NX_ERROR_BUSY;
    }
    for (size_t index = 0U; index < config->pin_count; ++index) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[index];
        uint32_t mask = 3U << (2U * pin->pin);
        if ((NX_I2C_TARGET_READ(&config->rcc->AHB1ENR) &
             (1U << pin->port_index)) != 0U) {
            uint32_t mode = NX_I2C_TARGET_READ(&pin->registers->MODER) & mask;
            if ((mode != 0U && mode != mask) ||
                (NX_I2C_TARGET_READ(&pin->registers->LCKR) &
                 (1U << pin->pin)) != 0U)
                return NX_ERROR_BUSY;
        }
    }
    return NX_SUCCESS;
}

nx_result_t
nx_stm32_i2c_target_prepare(nx_stm32_i2c_target_state_t* state,
                            const nx_stm32_i2c_target_config_t* config,
                            nx_stm32_i2c_target_cold_t* receipt) {
    nx_result_t result = nx_stm32_i2c_target_admit(state, config, receipt);
    if (result != NX_SUCCESS)
        return result;
    if (!clock_live(config))
        return NX_ERROR_STATE;
    state->config = config;
    state->cold = receipt;
    receipt->owner = state;
    receipt->owned = true;
    receipt->apb = NX_I2C_TARGET_READ(&config->rcc->APB1ENR) & gate(config);
    receipt->gpio_clocks = NX_I2C_TARGET_READ(&config->rcc->AHB1ENR);
    for (unsigned index = 0U; index < 2U; ++index)
        receipt->priorities[index] =
            (uint8_t)NX_I2C_TARGET_IRQ_PRIORITY(vector(config, index));
    receipt->vectors_captured = true;
    I2C_TypeDef* regs = config->registers;
    if (receipt->apb != 0U) {
        capture_registers(regs, receipt->registers);
        receipt->registers_captured = true;
    }
    if (!withdraw(state))
        return NX_ERROR_IO;
    NX_I2C_TARGET_WRITE(&config->rcc->APB1ENR,
                        NX_I2C_TARGET_READ(&config->rcc->APB1ENR) |
                            gate(config));
    nx_arch_dsb();
    if ((NX_I2C_TARGET_READ(&config->rcc->APB1ENR) & gate(config)) == 0U)
        return NX_ERROR_IO;
    if (!receipt->registers_captured) {
        capture_registers(regs, receipt->registers);
        receipt->registers_captured = true;
    }
    for (size_t index = 0U; index < config->pin_count; ++index) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[index];
        receipt->gpio_mask |= 1U << pin->port_index;
        NX_I2C_TARGET_WRITE(&config->rcc->AHB1ENR,
                            NX_I2C_TARGET_READ(&config->rcc->AHB1ENR) |
                                (1U << pin->port_index));
        nx_arch_dsb();
        if ((NX_I2C_TARGET_READ(&config->rcc->AHB1ENR) &
             (1U << pin->port_index)) == 0U)
            return NX_ERROR_IO;
        *pin->snapshot = nx_stm32_pin_capture(pin->registers, pin->pin);
        receipt->captured = (uint8_t)(index + 1U);
        result =
            nx_stm32_pin_configure(pin->registers, pin->pin, 2U, 4U, 0U, true);
        if (result != NX_SUCCESS ||
            (nx_stm32_pin_capture(pin->registers, pin->pin) & 0x7FFU) !=
                (2U | (1U << 2U) | (2U << 3U) | (4U << 7U)))
            return NX_ERROR_IO;
    }
    if (!program_controller(config))
        return NX_ERROR_IO;
    for (unsigned index = 0U; index < 2U; ++index) {
        IRQn_Type irq = vector(config, index);
        NX_I2C_TARGET_IRQ_PRIORITY_SET(irq, config->priority);
        NX_I2C_TARGET_IRQ_CLEAR(irq);
        NX_I2C_TARGET_IRQ_ENABLE(irq);
        nx_arch_dsb();
        if (NX_I2C_TARGET_IRQ_PRIORITY(irq) != config->priority ||
            NX_I2C_TARGET_IRQ_ENABLED(irq) == 0U ||
            NX_I2C_TARGET_IRQ_PENDING(irq) != 0U)
            return NX_ERROR_IO;
    }
    ++state->generation;
    state->time_epoch = g_nx_stm32_system.time_epoch;
    state->initialized = true;
    return NX_SUCCESS;
}

nx_result_t
nx_stm32_i2c_target_close(nx_stm32_i2c_target_state_t* state,
                          const nx_stm32_i2c_target_config_t* config,
                          nx_stm32_i2c_target_cold_t* receipt) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || !cold_storage(state, config, receipt))
        return NX_ERROR_INVALID;
    if (!receipt->owned)
        return state->config == NULL && receipt->owner == NULL ? NX_SUCCESS
                                                               : NX_ERROR_BUSY;
    if (state->config != config || state->cold != receipt ||
        receipt->owner != state)
        return NX_ERROR_INVALID;
    state->closing = true;
    if (state->active != NULL) {
        nx_result_t result = stop_owner(state);
        if (result != NX_SUCCESS)
            return result;
    }
    if (!withdraw(state))
        return NX_ERROR_IO;
    if (receipt->registers_captured &&
        !restore_registers(config->registers, receipt->registers))
        return NX_ERROR_IO;
    while (receipt->captured != 0U) {
        const nx_stm32_platform_pin_config_t* pin =
            &config->pins[receipt->captured - 1U];
        uint32_t mask = 3U << (2U * pin->pin);
        NX_I2C_TARGET_WRITE(&pin->registers->MODER,
                            NX_I2C_TARGET_READ(&pin->registers->MODER) | mask);
        nx_arch_dsb();
        if ((NX_I2C_TARGET_READ(&pin->registers->MODER) & mask) != mask)
            return NX_ERROR_IO;
        nx_stm32_pin_restore(pin->registers, pin->pin, *pin->snapshot);
        if (nx_stm32_pin_capture(pin->registers, pin->pin) != *pin->snapshot)
            return NX_ERROR_IO;
        --receipt->captured;
    }
    if (receipt->vectors_captured) {
        for (unsigned index = 0U; index < 2U; ++index) {
            IRQn_Type irq = vector(config, index);
            NX_I2C_TARGET_IRQ_PRIORITY_SET(irq, receipt->priorities[index]);
            if (NX_I2C_TARGET_IRQ_PRIORITY(irq) != receipt->priorities[index])
                return NX_ERROR_IO;
        }
        receipt->vectors_captured = false;
    }
    NX_I2C_TARGET_WRITE(
        &config->rcc->APB1ENR,
        (NX_I2C_TARGET_READ(&config->rcc->APB1ENR) & ~gate(config)) |
            receipt->apb);
    nx_arch_dsb();
    if ((NX_I2C_TARGET_READ(&config->rcc->APB1ENR) & gate(config)) !=
        receipt->apb)
        return NX_ERROR_IO;
    NX_I2C_TARGET_WRITE(
        &config->rcc->AHB1ENR,
        (NX_I2C_TARGET_READ(&config->rcc->AHB1ENR) & ~receipt->gpio_mask) |
            (receipt->gpio_clocks & receipt->gpio_mask));
    nx_arch_dsb();
    if ((NX_I2C_TARGET_READ(&config->rcc->AHB1ENR) & receipt->gpio_mask) !=
        (receipt->gpio_clocks & receipt->gpio_mask))
        return NX_ERROR_IO;
    uint64_t generation = state->generation;
    *state = (nx_stm32_i2c_target_state_t){.generation = generation};
    *receipt = (nx_stm32_i2c_target_cold_t){0};
    return NX_SUCCESS;
}

/** \brief A retained owner cannot carry deadlines through a System restart. */
static bool time_live(const nx_stm32_i2c_target_state_t* state) {
    return state->config != NULL && clock_live(state->config) &&
           state->time_epoch == g_nx_stm32_system.time_epoch &&
           (NX_I2C_TARGET_READ(&state->config->rcc->APB1ENR) &
            gate(state->config)) != 0U &&
           (NX_I2C_TARGET_READ(&state->config->rcc->APB1RSTR) &
            gate(state->config)) == 0U;
}

/** \brief Check actual RUN and peripheral controls before payload accesses. */
static bool run_live(const nx_stm32_i2c_target_state_t* state) {
    if (!time_live(state))
        return false;
    const nx_stm32_i2c_target_config_t* config = state->config;
    uint32_t expected = I2C_CR1_PE | (config->smbus ? I2C_CR1_SMBUS : 0U) |
                        (config->pec ? I2C_CR1_ENPEC : 0U);
    const uint32_t mask = I2C_CR1_PE | I2C_CR1_SMBUS | I2C_CR1_ENPEC |
                          I2C_CR1_NOSTRETCH | I2C_CR1_SMBTYPE | I2C_CR1_ENARP |
                          I2C_CR1_ENGC | I2C_CR1_SWRST | I2C_CR1_POS;
    return (NX_I2C_TARGET_READ(&config->registers->CR1) & mask) == expected &&
           (NX_I2C_TARGET_READ(&config->registers->CR2) &
            (I2C_CR2_FREQ | I2C_CR2_DMAEN)) == config->clock_hz / 1000000U;
}

/** \brief Metadata and payloads cannot mutate an owner or its static facts. */
static bool owner_span(const nx_stm32_i2c_target_state_t* state,
                       const void* pointer, size_t bytes, size_t alignment) {
    const nx_stm32_i2c_target_config_t* config = state->config;
    if (!span(pointer, bytes, alignment) ||
        hardware_overlap(config, pointer, bytes) ||
        overlap(pointer, bytes, state, sizeof(*state)) ||
        overlap(pointer, bytes, state->cold, sizeof(*state->cold)) ||
        overlap(pointer, bytes, config, sizeof(*config)) ||
        overlap(pointer, bytes, config->pins,
                config->pin_count * sizeof(*config->pins)))
        return false;
    for (size_t index = 0U; index < config->pin_count; ++index)
        if (overlap(pointer, bytes, config->pins[index].snapshot,
                    sizeof(uint16_t)))
            return false;
    return true;
}

/** \brief Whole finite wire shape is checked before writing READY metadata. */
static bool shape_valid(const nx_stm32_i2c_target_state_t* state,
                        const nx_i2c_target_request_t* request, uint8_t* rx,
                        size_t nrx, const uint8_t* tx, size_t ntx,
                        nx_i2c_target_direction_t direction) {
    if ((unsigned)direction > NX_I2C_TARGET_ALERT_RESPONSE ||
        nrx > NX_I2C_TARGET_MAX_BYTES || ntx > NX_I2C_TARGET_MAX_BYTES ||
        !owner_span(state, request, sizeof(*request),
                    _Alignof(nx_i2c_target_request_t)))
        return false;
    bool receive = direction == NX_I2C_TARGET_RECEIVE ||
                   direction == NX_I2C_TARGET_WRITE_READ;
    bool transmit = direction == NX_I2C_TARGET_TRANSMIT ||
                    direction == NX_I2C_TARGET_WRITE_READ;
    if ((receive != (nrx != 0U)) || (transmit != (ntx != 0U)) ||
        (receive ? !owner_span(state, rx, nrx, 1U) : rx != NULL) ||
        (transmit ? !owner_span(state, tx, ntx, 1U) : tx != NULL) ||
        overlap(request, sizeof(*request), rx, nrx) ||
        overlap(request, sizeof(*request), tx, ntx) ||
        overlap(rx, nrx, tx, ntx))
        return false;
    if (state->active != NULL &&
        (overlap(request, sizeof(*request), state->active,
                 sizeof(*state->active)) ||
         overlap(rx, nrx, state->active, sizeof(*state->active)) ||
         overlap(tx, ntx, state->active, sizeof(*state->active)) ||
         overlap(request, sizeof(*request), state->active->rx,
                 state->active->rx_capacity) ||
         overlap(request, sizeof(*request), state->active->tx,
                 state->active->tx_length) ||
         overlap(rx, nrx, state->active->rx, state->active->rx_capacity) ||
         overlap(rx, nrx, state->active->tx, state->active->tx_length) ||
         overlap(tx, ntx, state->active->rx, state->active->rx_capacity) ||
         overlap(tx, ntx, state->active->tx, state->active->tx_length)))
        return false;
    return direction != NX_I2C_TARGET_ALERT_RESPONSE || state->config->alert;
}

/** \brief Observe Core time only while its exact System epoch remains live. */
static bool observe_time(nx_stm32_i2c_target_state_t* state,
                         nx_time_us_t* now) {
    if (!time_live(state))
        return false;
    *now = nx_stm32_system_now(&g_nx_stm32_system);
    return *now != NX_DEADLINE_NEVER && time_live(state);
}

/** \brief A completion flag's entry timestamp is only a lower time bound. */
static nx_result_t completion_result(nx_stm32_i2c_target_state_t* state) {
    nx_time_us_t now;
    if (!observe_time(state, &now))
        return NX_ERROR_IO;
    return now >= state->active->base.deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}

/** \brief READY identities are pure metadata; address ACK remains disabled. */
static nx_result_t
prepare_request(void* opaque, nx_i2c_target_request_t* request, uint8_t* rx,
                size_t nrx, const uint8_t* tx, size_t ntx,
                nx_i2c_target_direction_t direction, nx_time_us_t deadline) {
    nx_stm32_i2c_target_state_t* state = opaque;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!span(state, sizeof(*state), _Alignof(nx_stm32_i2c_target_state_t)) ||
        state->config == NULL || !state->initialized || state->closing ||
        state->faulted || !time_live(state))
        return NX_ERROR_STATE;
    if (deadline == NX_DEADLINE_NEVER ||
        !shape_valid(state, request, rx, nrx, tx, ntx, direction))
        return NX_ERROR_INVALID;
    if (nx_request_state(&request->base) != NX_REQUEST_READY &&
        nx_request_state(&request->base) != NX_REQUEST_SETTLED)
        return NX_ERROR_BUSY;
    nx_time_us_t now;
    if (!observe_time(state, &now))
        return NX_ERROR_STATE;
    if (deadline <= now)
        return NX_ERROR_TIMEOUT;
    nx_result_t result = nx_request_prepare(&request->base, deadline);
    if (result != NX_SUCCESS)
        return result;
    request->owner = state;
    request->generation = state->generation;
    request->time_epoch = state->time_epoch;
    request->rx = rx;
    request->rx_capacity = nrx;
    request->tx = tx;
    request->tx_length = ntx;
    request->direction = direction;
    request->rx_received = 0U;
    request->tx_completed = 0U;
    request->faults = 0U;
    request->matched_address = 0U;
    request->address_phases = 0U;
    return NX_SUCCESS;
}

/** \brief Preserve the first terminal result and borrow while withdrawing. */
static void begin_drain(nx_stm32_i2c_target_state_t* state,
                        nx_result_t result) {
    if (!state->terminal_latched) {
        state->terminal = result;
        state->terminal_latched = true;
    }
    if (state->active == NULL)
        return;
    if (result == NX_ERROR_IO)
        state->faulted = true;
    (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
    /* A pending PEC forbids any CR1 write (RM0090 27.6.1). Only turn off
     * publishers here; bounded Thread reset later proves PE/ACK/ALERT quiet. */
    NX_I2C_TARGET_WRITE(&state->config->registers->CR2,
                        state->config->clock_hz / 1000000U);
}

/** \brief Re-arm exact vectors only after acceptance owns request storage. */
static bool arm_vectors(const nx_stm32_i2c_target_config_t* config) {
    for (unsigned index = 0U; index < 2U; ++index) {
        IRQn_Type irq = vector(config, index);
        NX_I2C_TARGET_IRQ_CLEAR(irq);
        NX_I2C_TARGET_IRQ_ENABLE(irq);
        nx_arch_dsb();
        if (NX_I2C_TARGET_IRQ_ENABLED(irq) == 0U ||
            NX_I2C_TARGET_IRQ_PENDING(irq) != 0U)
            return false;
    }
    return true;
}

/** \brief Accept once before source setup; setup errors retain the borrow. */
static nx_result_t submit_request(void* opaque,
                                  nx_i2c_target_request_t* request) {
    nx_stm32_i2c_target_state_t* state = opaque;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!span(state, sizeof(*state), _Alignof(nx_stm32_i2c_target_state_t)) ||
        state->config == NULL || !state->initialized)
        return NX_ERROR_STATE;
    if (state->active != NULL)
        return NX_ERROR_BUSY;
    if (state->closing || state->faulted || !time_live(state))
        return NX_ERROR_STATE;
    if (!owner_span(state, request, sizeof(*request),
                    _Alignof(nx_i2c_target_request_t)))
        return NX_ERROR_INVALID;
    if (request->owner != state || request->generation != state->generation ||
        request->time_epoch != state->time_epoch)
        return NX_ERROR_STATE;
    if (nx_request_state(&request->base) != NX_REQUEST_READY)
        return NX_ERROR_STATE;
    if (request->base.deadline == NX_DEADLINE_NEVER ||
        !shape_valid(state, request, request->rx, request->rx_capacity,
                     request->tx, request->tx_length, request->direction))
        return NX_ERROR_INVALID;
    nx_time_us_t now;
    if (!observe_time(state, &now))
        return NX_ERROR_STATE;
    if (now >= request->base.deadline)
        return NX_ERROR_TIMEOUT;
    I2C_TypeDef* regs = state->config->registers;
    uint32_t cr1 = NX_I2C_TARGET_READ(&regs->CR1);
    if ((cr1 & (I2C_CR1_ACK | I2C_CR1_PEC | I2C_CR1_ALERT | I2C_CR1_START |
                I2C_CR1_STOP)) != 0U ||
        (NX_I2C_TARGET_READ(&regs->CR2) & (I2C_CR2_ITERREN | I2C_CR2_ITEVTEN |
                                           I2C_CR2_ITBUFEN | I2C_CR2_DMAEN)) !=
            0U ||
        NX_I2C_TARGET_READ(&regs->SR1) != 0U)
        return NX_ERROR_BUSY;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!time_live(state)) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    nx_result_t result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        nx_arch_irq_restore(saved);
        return result;
    }
    state->active = request;
    state->received = state->loaded = state->transmitted = 0U;
    state->faults = 0U;
    state->matched_address = 0U;
    state->phases = state->crc = 0U;
    state->terminal_latched = state->transmitter = false;
    state->pec_started = state->pec_received = state->ara_started = false;
    if (!reset_controller(state) || !program_controller(state->config) ||
        !arm_vectors(state->config)) {
        begin_drain(state, NX_ERROR_IO);
    } else {
        bool ara = request->direction == NX_I2C_TARGET_ALERT_RESPONSE;
        uint32_t control = NX_I2C_TARGET_READ(&regs->CR1) |
                           (ara ? I2C_CR1_ALERT : I2C_CR1_ACK);
        uint32_t sources = state->config->clock_hz / 1000000U |
                           I2C_CR2_ITERREN | I2C_CR2_ITEVTEN;
        if (request->direction == NX_I2C_TARGET_RECEIVE ||
            request->direction == NX_I2C_TARGET_WRITE_READ ||
            state->config->ten_bit)
            sources |= I2C_CR2_ITBUFEN;
        NX_I2C_TARGET_WRITE(&regs->CR2, sources);
        NX_I2C_TARGET_WRITE(&regs->CR1, control);
        nx_arch_dsb();
        if (!run_live(state) || NX_I2C_TARGET_READ(&regs->CR2) != sources ||
            NX_I2C_TARGET_READ(&regs->CR1) != control)
            begin_drain(state, NX_ERROR_IO);
    }
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief CRC-8 is independent of SR2, whose read can clear a late ADDR. */
static uint8_t crc_byte(uint8_t crc, uint8_t value) {
    crc ^= value;
    for (unsigned bit = 0U; bit < 8U; ++bit)
        crc = (uint8_t)((crc & 0x80U) != 0U ? ((uint32_t)crc << 1U) ^ 7U
                                            : (uint32_t)crc << 1U);
    return crc;
}

/** \brief PEC command is never replayed by stale CR1 read-modify-write. */
static bool request_pec(nx_stm32_i2c_target_state_t* state) {
    I2C_TypeDef* regs = state->config->registers;
    uint32_t control = NX_I2C_TARGET_READ(&regs->CR1);
    if ((control & I2C_CR1_PEC) != 0U)
        return false;
    bool ara = state->active->direction == NX_I2C_TARGET_ALERT_RESPONSE;
    if (ara)
        control &= ~(uint32_t)I2C_CR1_ALERT;
    NX_I2C_TARGET_WRITE(&regs->CR1, control | I2C_CR1_PEC);
    nx_arch_dsb();
    /* Under the admitted sub-six-bit work bound, this immediate observation
     * precedes the next complete PEC byte. Refusal cannot be replaced by a
     * software CRC match. Readback still does not measure the physical ACK
     * window or establish that integration respected its declared budget. */
    uint32_t observed = NX_I2C_TARGET_READ(&regs->CR1);
    if ((observed & I2C_CR1_PEC) == 0U ||
        (ara && (observed & I2C_CR1_ALERT) != 0U))
        return false;
    state->pec_started = true;
    return run_live(state);
}

/** \brief Feed one stretched transmitter slot, without speculative prefetch.
 */
static void transmit_byte(nx_stm32_i2c_target_state_t* state) {
    nx_i2c_target_request_t* request = state->active;
    bool ara = request->direction == NX_I2C_TARGET_ALERT_RESPONSE;
    size_t length = ara ? 1U : request->tx_length;
    if (!run_live(state)) {
        begin_drain(state, NX_ERROR_IO);
        return;
    }
    if (state->loaded >= length) {
        state->transmitted = state->loaded;
        if (state->config->pec && !state->pec_started && request_pec(state))
            return;
        state->faults |= NX_I2C_TARGET_PROTOCOL_ERROR;
        begin_drain(state, NX_ERROR_IO);
        return;
    }
    uint8_t value = (uint8_t)(ara ? state->config->own_address << 1U
                                  : request->tx[state->loaded]);
    if (!run_live(state)) {
        begin_drain(state, NX_ERROR_IO);
        return;
    }
    /* The selected SDK's LL TransmitData8 does a DR read-modify-write. DR is
     * a destructive data port, not a control register: issue exactly one
     * byte store after the final domain check, with no implicit DR read. */
    NX_I2C_TARGET_WRITE(&state->config->registers->DR, value);
    state->crc = crc_byte(state->crc, value);
    ++state->loaded;
}

/** \brief Drain at most the two real pipeline bytes before clearing STOPF. */
static void receive_bytes(nx_stm32_i2c_target_state_t* state, uint32_t status) {
    nx_i2c_target_request_t* request = state->active;
    I2C_TypeDef* regs = state->config->registers;
    for (unsigned count = 0U; count < 2U && (status & I2C_SR1_RXNE) != 0U;
         ++count) {
        if (state->transmitter || state->phases == 0U ||
            request->direction == NX_I2C_TARGET_TRANSMIT ||
            request->direction == NX_I2C_TARGET_ALERT_RESPONSE ||
            !run_live(state)) {
            state->faults |= NX_I2C_TARGET_PROTOCOL_ERROR;
            begin_drain(state, NX_ERROR_IO);
            return;
        }
        uint8_t value = LL_I2C_ReceiveData8(regs);
        if (!run_live(state)) {
            begin_drain(state, NX_ERROR_IO);
            return;
        }
        if (state->received < request->rx_capacity) {
            request->rx[state->received++] = value;
            state->crc = crc_byte(state->crc, value);
            if (state->config->pec &&
                request->direction == NX_I2C_TARGET_RECEIVE &&
                state->received == request->rx_capacity) {
                if ((status & I2C_SR1_BTF) != 0U || !request_pec(state)) {
                    state->faults |= NX_I2C_TARGET_PROTOCOL_ERROR;
                    begin_drain(state, NX_ERROR_IO);
                    return;
                }
            }
        } else if (state->config->pec &&
                   request->direction == NX_I2C_TARGET_RECEIVE &&
                   state->pec_started && !state->pec_received) {
            state->pec_received = true;
            if (state->crc != value) {
                state->faults |= NX_I2C_TARGET_PEC_ERROR;
                begin_drain(state, NX_ERROR_IO);
                return;
            }
        } else {
            state->faults |= NX_I2C_TARGET_OVERRUN;
            begin_drain(state, NX_ERROR_IO);
            return;
        }
        status = NX_I2C_TARGET_READ(&regs->SR1);
    }
}

/** \brief Observed ADDR alone permits the destructive SR1/SR2 sequence. */
static void address_match(nx_stm32_i2c_target_state_t* state) {
    nx_i2c_target_request_t* request = state->active;
    const nx_stm32_i2c_target_config_t* config = state->config;
    uint32_t facts = NX_I2C_TARGET_READ(&config->registers->SR2);
    bool transmitter = (facts & I2C_SR2_TRA) != 0U;
    uint16_t address = (facts & I2C_SR2_DUALF) != 0U ? config->second_address
                                                     : config->own_address;
    bool first = state->phases == 0U;
    bool preamble =
        config->ten_bit && request->direction == NX_I2C_TARGET_TRANSMIT;
    bool expected =
        first ? (transmitter
                     ? request->direction == NX_I2C_TARGET_TRANSMIT &&
                           !config->ten_bit
                     : request->direction == NX_I2C_TARGET_RECEIVE ||
                           request->direction == NX_I2C_TARGET_WRITE_READ ||
                           preamble)
              : state->phases == 1U && transmitter && !state->transmitter &&
                    address == state->matched_address &&
                    (request->direction == NX_I2C_TARGET_WRITE_READ ||
                     (preamble && state->received == 0U));
    if (!expected || (!config->ten_bit && address == 0U) || !run_live(state)) {
        state->faults |= NX_I2C_TARGET_PROTOCOL_ERROR;
        begin_drain(state, NX_ERROR_IO);
        return;
    }
    ++state->phases;
    state->transmitter = transmitter;
    state->matched_address = address;
    if (!config->ten_bit)
        state->crc = crc_byte(state->crc, (uint8_t)((uint32_t)address << 1U |
                                                    (transmitter ? 1U : 0U)));
    if (transmitter) {
        NX_I2C_TARGET_WRITE(&config->registers->CR2,
                            config->clock_hz / 1000000U | I2C_CR2_ITERREN |
                                I2C_CR2_ITEVTEN);
        transmit_byte(state);
    }
}

/** \brief Hardware errors remain independent diagnostics from byte counts. */
static uint32_t error_facts(uint32_t status) {
    return ((status & I2C_SR1_BERR) ? NX_I2C_TARGET_BUS_ERROR : 0U) |
           ((status & I2C_SR1_ARLO) ? NX_I2C_TARGET_ARBITRATION : 0U) |
           ((status & I2C_SR1_OVR) ? NX_I2C_TARGET_OVERRUN : 0U) |
           ((status & I2C_SR1_PECERR) ? NX_I2C_TARGET_PEC_ERROR : 0U) |
           ((status & I2C_SR1_TIMEOUT) ? NX_I2C_TARGET_CLOCK_TIMEOUT : 0U);
}

void nx_stm32_i2c_target_irq(nx_stm32_i2c_target_state_t* state) {
    if (state == NULL || !state->initialized || state->config == NULL ||
        state->active == NULL || state->terminal_latched)
        return;
    if (!run_live(state)) {
        begin_drain(state, NX_ERROR_IO);
        return;
    }
    nx_time_us_t now;
    if (!observe_time(state, &now)) {
        begin_drain(state, NX_ERROR_IO);
        return;
    }
    if (now >= state->active->base.deadline) {
        begin_drain(state, NX_ERROR_TIMEOUT);
        return;
    }
    I2C_TypeDef* regs = state->config->registers;
    uint32_t status = NX_I2C_TARGET_READ(&regs->SR1);
    uint32_t errors = status & (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR |
                                I2C_SR1_PECERR | I2C_SR1_TIMEOUT);
    bool ara = state->active->direction == NX_I2C_TARGET_ALERT_RESPONSE;
    if (errors != 0U) {
        state->faults |= error_facts(errors);
        NX_I2C_TARGET_WRITE(&regs->SR1, ~errors);
        if (errors == I2C_SR1_ARLO && ara) {
            if (state->config->pec) {
                /* RM0090 27.6.1: ARLO corrupts the hardware PEC calculator.
                 * Reset only our peripheral, not the shared bus or its other
                 * targets. The current IRQ still owns its accepted descriptor;
                 * this is an internal ARA retry, never full withdrawal. */
                if (!reset_controller(state) ||
                    !program_controller(state->config)) {
                    begin_drain(state, NX_ERROR_IO);
                    return;
                }
                NX_I2C_TARGET_WRITE(&regs->CR2,
                                    state->config->clock_hz / 1000000U |
                                        I2C_CR2_ITERREN | I2C_CR2_ITEVTEN);
                NX_I2C_TARGET_WRITE(&regs->CR1, NX_I2C_TARGET_READ(&regs->CR1) |
                                                    I2C_CR1_ALERT);
                if (!run_live(state) ||
                    (NX_I2C_TARGET_READ(&regs->CR1) & I2C_CR1_ALERT) == 0U) {
                    begin_drain(state, NX_ERROR_IO);
                    return;
                }
            }
            state->loaded = state->transmitted = 0U;
            state->ara_started = state->transmitter = false;
            state->pec_started = state->pec_received = false;
            state->crc = 0U;
            return;
        }
        begin_drain(state, errors == I2C_SR1_ARLO      ? NX_ERROR_ARBITRATION
                           : errors == I2C_SR1_TIMEOUT ? NX_ERROR_TIMEOUT
                                                       : NX_ERROR_IO);
        return;
    }
    /* A repeated START can arrive while the old RX pipeline is still full.
     * Consume that old direction before the new ADDR/SR2 releases SCL and
     * turns the controller into a transmitter. The fresh SR1 observation is
     * essential: no SR2 read follows an earlier snapshot without ADDR. */
    if ((status & (I2C_SR1_ADDR | I2C_SR1_RXNE)) ==
            (I2C_SR1_ADDR | I2C_SR1_RXNE) &&
        state->phases != 0U && !state->transmitter) {
        receive_bytes(state, status);
        if (state->terminal_latched)
            return;
        status = NX_I2C_TARGET_READ(&regs->SR1);
    }
    if ((status & I2C_SR1_SMBALERT) != 0U) {
        NX_I2C_TARGET_WRITE(&regs->SR1, ~(uint32_t)I2C_SR1_SMBALERT);
        if (!ara || state->ara_started) {
            state->faults |= NX_I2C_TARGET_PROTOCOL_ERROR;
            begin_drain(state, NX_ERROR_IO);
            return;
        }
        state->ara_started = state->transmitter = true;
        state->matched_address = state->config->own_address;
        state->phases = 1U;
        state->crc = crc_byte(0U, 0x19U);
        transmit_byte(state);
    } else if ((status & I2C_SR1_ADDR) != 0U) {
        address_match(state);
    }
    if (state->terminal_latched)
        return;
    if ((status & I2C_SR1_RXNE) != 0U)
        receive_bytes(state, status);
    if (state->terminal_latched)
        return;
    if ((status & I2C_SR1_AF) != 0U) {
        NX_I2C_TARGET_WRITE(&regs->SR1, ~(uint32_t)I2C_SR1_AF);
        size_t length = ara ? 1U : state->active->tx_length;
        bool complete = state->transmitter && state->loaded == length &&
                        (!state->config->pec || state->pec_started);
        if (state->transmitter && state->loaded != 0U)
            state->transmitted = state->loaded;
        if (complete && ara && !state->config->pec) {
            uint32_t control = NX_I2C_TARGET_READ(&regs->CR1);
            NX_I2C_TARGET_WRITE(&regs->CR1, control & ~(uint32_t)I2C_CR1_ALERT);
            nx_arch_dsb();
            if ((NX_I2C_TARGET_READ(&regs->CR1) & I2C_CR1_ALERT) != 0U)
                complete = false;
        }
        if (!complete)
            state->faults |= NX_I2C_TARGET_PROTOCOL_ERROR;
        begin_drain(state, complete ? completion_result(state) : NX_ERROR_IO);
    } else if ((status & I2C_SR1_STOPF) != 0U) {
        /* Re-read SR1 after the final DR reads. CR1 is a fresh noncommand
         * tuple, so a cleared PEC cannot be accidentally requested again. */
        (void)NX_I2C_TARGET_READ(&regs->SR1);
        uint32_t control = NX_I2C_TARGET_READ(&regs->CR1);
        if ((control & I2C_CR1_PEC) != 0U) {
            begin_drain(state, NX_ERROR_IO);
            return;
        }
        NX_I2C_TARGET_WRITE(&regs->CR1,
                            control & ~(uint32_t)(I2C_CR1_ACK | I2C_CR1_ALERT));
        bool complete = state->phases == 1U && !state->transmitter &&
                        state->active->direction == NX_I2C_TARGET_RECEIVE &&
                        (!state->config->pec ||
                         (state->pec_received &&
                          state->received == state->active->rx_capacity));
        if (!complete)
            state->faults |= NX_I2C_TARGET_PROTOCOL_ERROR;
        begin_drain(state, complete ? completion_result(state) : NX_ERROR_IO);
    } else if ((status & I2C_SR1_BTF) != 0U && state->transmitter) {
        state->transmitted = state->loaded;
        transmit_byte(state);
    }
}

/** \brief Service alone can publish after reset, NVIC and final epoch proof. */
static void service_owner(void* opaque) {
    nx_stm32_i2c_target_state_t* state = opaque;
    if (!task_context() || state == NULL || state->config == NULL ||
        state->active == NULL)
        return;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!state->terminal_latched) {
        nx_time_us_t now;
        if (!observe_time(state, &now) || !run_live(state))
            begin_drain(state, NX_ERROR_IO);
        else if (now >= state->active->base.deadline)
            begin_drain(state, NX_ERROR_TIMEOUT);
    }
    if (!state->terminal_latched) {
        nx_arch_irq_restore(saved);
        return;
    }
    if (!withdraw(state)) {
        (void)nx_request_transition(&state->active->base,
                                    NX_REQUEST_QUARANTINED);
        nx_arch_irq_restore(saved);
        return;
    }
    if (state->terminal == NX_SUCCESS) {
        state->terminal = completion_result(state);
        if (state->terminal == NX_ERROR_IO)
            state->faulted = true;
    }
    nx_i2c_target_request_t* request = state->active;
    request->rx_received = state->received;
    request->tx_completed = state->transmitted;
    request->faults = state->faults;
    request->matched_address = state->matched_address;
    request->address_phases = state->phases;
    nx_result_t terminal = state->terminal;
    size_t transferred = state->received + state->transmitted;
    state->active = NULL;
    nx_request_settle(&request->base, terminal, transferred);
    nx_arch_irq_restore(saved);
}

/** \brief Cancel is a terminal request, never a revocation of caller storage.
 */
static nx_result_t cancel_request(void* opaque,
                                  nx_i2c_target_request_t* request) {
    nx_stm32_i2c_target_state_t* state = opaque;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || state->config == NULL || state->active != request)
        return NX_ERROR_STATE;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    begin_drain(state, NX_ERROR_CANCELLED);
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief Stop closes admission and uses the same bounded borrow withdrawal.
 */
static nx_result_t stop_owner(void* opaque) {
    nx_stm32_i2c_target_state_t* state = opaque;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || state->config == NULL)
        return NX_ERROR_STATE;
    state->closing = true;
    if (state->active != NULL) {
        (void)cancel_request(state, state->active);
        service_owner(state);
        if (state->active != NULL)
            return NX_ERROR_BUSY;
    }
    return withdraw(state) ? NX_SUCCESS : NX_ERROR_IO;
}

/** \brief Reopen only this live epoch, advancing READY identity without replay.
 */
static nx_result_t recover_owner(void* opaque) {
    nx_stm32_i2c_target_state_t* state = opaque;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || state->config == NULL || !state->initialized ||
        state->closing || state->cold == NULL || !state->cold->owned ||
        state->cold->captured != state->config->pin_count ||
        !state->cold->vectors_captured || !time_live(state))
        return NX_ERROR_STATE;
    if (state->active != NULL)
        return NX_ERROR_BUSY;
    if (state->generation == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    if (!withdraw(state) || !program_controller(state->config) ||
        !arm_vectors(state->config))
        return NX_ERROR_IO;
    ++state->generation;
    state->closing = state->faulted = false;
    return NX_SUCCESS;
}

const nx_i2c_target_ops_t nx_stm32_i2c_target_ops = {.prepare = prepare_request,
                                                     .submit = submit_request,
                                                     .cancel = cancel_request,
                                                     .service = service_owner,
                                                     .stop = stop_owner,
                                                     .recover = recover_owner};
