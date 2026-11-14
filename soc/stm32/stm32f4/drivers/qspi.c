/**
 * \file            qspi.c
 * \brief           Official QSPI transport with explicit physical Flash exit
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
#include "stm32f4_qspi.h"
#if NEXUS_QSPI_CPU_MAP
#include "stm32f4_qspi_mapped.h"
#endif
#include "stm32f4xx_ll_gpio.h"
#include <string.h>

#if !defined(STM32F479xx)
#error "This indirect endpoint requires the reviewed F479 selection"
#endif
extern nx_stm32_system_t g_nx_stm32_system;
enum {
    IDLE,
    STATUS1,
    STATUS2,
    STATUS3,
    ENABLE_WRITE,
    CHECK_WEL,
    PULSE,
    POLL_READY,
    VERIFY,
    DISABLE_WRITE,
    DRAIN_WIRE
};
static const uint32_t interrupt_bits = QUADSPI_CR_TEIE | QUADSPI_CR_TCIE |
                                       QUADSPI_CR_FTIE | QUADSPI_CR_SMIE |
                                       QUADSPI_CR_TOIE;
static const uint32_t terminal_bits =
    QUADSPI_SR_TEF | QUADSPI_SR_TCF | QUADSPI_SR_SMF | QUADSPI_SR_TOF;
static const nx_flash_erase_span_t erase_span = {0U, 4096U, 4096U};
static const nx_flash_geometry_t physical = {0U,          0x1000000U, 1U,
                                             &erase_span, 1U,         0U};
static const struct {
    uint8_t port;
    uint8_t pad;
    uint8_t af;
} pin_facts[] = {{1U, 6U, 10U}, {5U, 10U, 9U}, {5U, 8U, 10U}, {5U, 9U, 10U},
#if NEXUS_QSPI_QUAD_READ
                 {5U, 7U, 9U},  {5U, 6U, 9U}
#endif
};
#if NEXUS_QSPI_QUAD_READ
static const uint8_t read_instruction = 0x6bU;
#else
static const uint8_t read_instruction = 0x03U;
#endif
_Static_assert(offsetof(nx_stm32_qspi_t, transport) == 0U,
               "A callback belongs to its first-member transport owner");

static bool context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool span(const void* object, size_t bytes, size_t alignment,
                 bool write) {
    uintptr_t begin = (uintptr_t)object;
    if (object == NULL || begin % alignment != 0U ||
        bytes > UINTPTR_MAX - begin)
        return false;
    uint64_t first = begin, end = first + bytes;
    return !(first < UINT64_C(0x100000000) && UINT64_C(0x40000000) < end) &&
           (!write ||
            !(first < UINT64_C(0x10000000) && UINT64_C(0x08000000) < end));
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
static bool valid(const nx_stm32_qspi_t* owner) {
    return span(owner, sizeof(*owner), _Alignof(nx_stm32_qspi_t), true);
}
static bool epoch_matches(const nx_stm32_qspi_config_t* config,
                          uint64_t epoch) {
    const nx_stm32_system_t* system = config->system;
    return system == &g_nx_stm32_system && system->rcc == RCC &&
           system->flash == FLASH && system->power == PWR &&
           system->timer == TIM2 && system->core_hz == NEXUS_CORE_HZ &&
           system->started && system->timer_error == NX_SUCCESS &&
           system->time_epoch != 0U && system->time_epoch == epoch;
}
static bool epoch_live(const nx_stm32_qspi_t* owner) {
    return epoch_matches(owner->config, owner->system_epoch);
}
static nx_result_t clock_value(const nx_stm32_qspi_config_t* config,
                               uint64_t epoch, nx_time_us_t* now) {
    if (!epoch_matches(config, epoch) ||
        !nx_stm32_clock_profile_matches(RCC, FLASH))
        return NX_ERROR_STATE;
    *now = nx_stm32_system_clock_read(config->system);
    return epoch_matches(config, epoch) && *now != NX_DEADLINE_NEVER
               ? NX_SUCCESS
               : NX_ERROR_STATE;
}
static nx_result_t clock_read(nx_stm32_qspi_t* owner, nx_time_us_t* now) {
    if ((READ_REG(RCC->AHB3ENR) & RCC_AHB3ENR_QSPIEN) == 0U)
        return NX_ERROR_STATE;
    nx_result_t result = clock_value(owner->config, owner->system_epoch, now);
    if (result != NX_SUCCESS || *now < owner->last_clock)
        return NX_ERROR_STATE;
    owner->last_clock = *now;
    return NX_SUCCESS;
}
static bool isolated(const nx_stm32_qspi_t* owner, const void* object,
                     size_t bytes, size_t align, bool write) {
    const nx_stm32_qspi_config_t* config = owner->config;
    if (!span(object, bytes, align, write) ||
        overlap(object, bytes, owner, sizeof(*owner)) ||
        overlap(object, bytes, config, sizeof(*config)) ||
        overlap(object, bytes, config->system, sizeof(*config->system)) ||
        overlap(object, bytes, config->pins,
                config->pin_count * sizeof(*config->pins)))
        return false;
#if NEXUS_QSPI_PROGRAM_DMA
    const nx_stm32_qspi_dma_config_t* dma = config->dma;
    if (dma != NULL &&
        (overlap(object, bytes, dma, sizeof(*dma)) ||
         overlap(object, bytes, dma->state, sizeof(*dma->state)) ||
         overlap(object, bytes, dma->regions,
                 dma->region_count * sizeof(*dma->regions))))
        return false;
#endif
    for (size_t i = 0U; i < config->pin_count; ++i)
        if (overlap(object, bytes, config->pins[i].snapshot, sizeof(uint16_t)))
            return false;
    return true;
}
static bool irq_quiet(void) {
    return NVIC_GetEnableIRQ(QUADSPI_IRQn) == 0U &&
           NVIC_GetActive(QUADSPI_IRQn) == 0U;
}
static bool hardware_idle(void) {
    return (READ_REG(QUADSPI->SR) & (QUADSPI_SR_BUSY | QUADSPI_SR_FLEVEL)) ==
               0U &&
           (READ_REG(QUADSPI->CR) & (QUADSPI_CR_ABORT | QUADSPI_CR_DMAEN)) ==
               0U;
}
static void complete(QSPI_HandleTypeDef* handle) {
    ((nx_stm32_qspi_t*)handle)->complete = true;
}
static void transport_error(QSPI_HandleTypeDef* handle) {
    ((nx_stm32_qspi_t*)handle)->transport_error = true;
}
static void noop(QSPI_HandleTypeDef* handle) {
    (void)handle;
}
/** \brief Forget bytes only after task-owned sources and optional DMA join.
 */
static void forget_buffers(nx_stm32_qspi_t* owner) {
    owner->transport.pTxBuffPtr = NULL;
    owner->transport.pRxBuffPtr = NULL;
    owner->transport.TxXferCount = owner->transport.TxXferSize = 0U;
    owner->transport.RxXferCount = owner->transport.RxXferSize = 0U;
}
static nx_result_t quiet_sources(nx_stm32_qspi_t* owner) {
    if (!irq_quiet() || (READ_REG(RCC->AHB3ENR) & RCC_AHB3ENR_QSPIEN) == 0U)
        return NX_ERROR_STATE;
    __HAL_QSPI_DISABLE_IT(&owner->transport, interrupt_bits);
#if NEXUS_QSPI_PROGRAM_DMA
    nx_result_t result = nx_stm32_qspi_dma_withdraw(owner);
    if (result != NX_SUCCESS)
        return result;
#endif
    nx_arch_dsb();
    if ((READ_REG(QUADSPI->CR) & (interrupt_bits | QUADSPI_CR_DMAEN)) != 0U)
        return NX_ERROR_IO;
    forget_buffers(owner);
    return NX_SUCCESS;
}
/** \brief RM0386 13.3.14: ABORT completion proves BUSY0 and FIFO flushed. */
static nx_result_t wire_abort(nx_stm32_qspi_t* owner) {
    nx_result_t result = quiet_sources(owner);
    if (result != NX_SUCCESS)
        return result;
    if (!hardware_idle()) {
        if ((READ_REG(QUADSPI->CR) & QUADSPI_CR_ABORT) == 0U) {
            /* HAL skips a BUSY0 residual FIFO. This exact cleanup gap uses
             * the documented controller ABORT, never a shared DMA reset. */
            SET_BIT(QUADSPI->CR, QUADSPI_CR_ABORT);
            nx_arch_dsb();
        }
        return NX_ERROR_BUSY;
    }
    __HAL_QSPI_CLEAR_FLAG(&owner->transport, terminal_bits);
    nx_arch_dsb();
    if ((READ_REG(QUADSPI->SR) & terminal_bits) != 0U)
        return NX_ERROR_IO;
    NVIC_ClearPendingIRQ(QUADSPI_IRQn);
    nx_arch_dsb();
    if (NVIC_GetPendingIRQ(QUADSPI_IRQn) != 0U || !irq_quiet())
        return NX_ERROR_IO;
    owner->transport.State = HAL_QSPI_STATE_READY;
    owner->wire_active = false;
    return NX_SUCCESS;
}
static nx_result_t wire_start(nx_stm32_qspi_t* owner, uint8_t instruction,
                              uint32_t address, bool addressed, uint8_t* data,
                              size_t bytes, bool transmit,
                              nx_time_us_t deadline) {
    nx_time_us_t now;
    nx_result_t result = clock_read(owner, &now);
    if (result != NX_SUCCESS || deadline == NX_DEADLINE_NEVER ||
        now >= deadline)
        return result != NX_SUCCESS ? result : NX_ERROR_TIMEOUT;
    if (!hardware_idle() || !irq_quiet() || owner->wire_active ||
        (READ_REG(QUADSPI->CR) & interrupt_bits) != 0U ||
        NVIC_GetPendingIRQ(QUADSPI_IRQn) != 0U ||
        owner->transport.State != HAL_QSPI_STATE_READY)
        return NX_ERROR_BUSY;
    QSPI_CommandTypeDef command = {0};
    command.Instruction = instruction;
    command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command.Address = address;
    command.AddressMode = addressed ? QSPI_ADDRESS_1_LINE : QSPI_ADDRESS_NONE;
    command.AddressSize = QSPI_ADDRESS_24_BITS;
    command.DataMode = bytes == 0U ? QSPI_DATA_NONE : QSPI_DATA_1_LINE;
#if NEXUS_QSPI_QUAD_READ
    if (instruction == read_instruction) {
        command.DataMode = QSPI_DATA_4_LINES;
        command.DummyCycles = 8U;
    }
#endif
    command.NbData = (uint32_t)bytes;
    command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    owner->complete = false;
    owner->transport_error = false;
    owner->wire_active = true; /* CCR/AR may now issue an actual frame. */
    HAL_StatusTypeDef status = HAL_QSPI_Command_IT(&owner->transport, &command);
    uint32_t expected = instruction | QSPI_INSTRUCTION_1_LINE |
                        command.AddressMode | command.DataMode |
                        (addressed ? QSPI_ADDRESS_24_BITS : 0U) |
                        command.DummyCycles << QUADSPI_CCR_DCYC_Pos;
    /* HAL takes its buffer counts from DLR, not the requested byte span.
     * Prove that value before assigning caller pointers or starting data. */
    if (status == HAL_OK &&
        (READ_REG(QUADSPI->CCR) != expected ||
         (addressed && READ_REG(QUADSPI->AR) != address) ||
         (bytes != 0U && READ_REG(QUADSPI->DLR) != bytes - 1U)))
        status = HAL_ERROR;
    if (status == HAL_OK && bytes != 0U) {
#if NEXUS_QSPI_PROGRAM_DMA
        if (transmit && owner->config->dma != NULL)
            status = nx_stm32_qspi_dma_transmit(owner, data) == NX_SUCCESS
                         ? HAL_OK
                         : HAL_ERROR;
        else
#endif
            status = transmit ? HAL_QSPI_Transmit_IT(&owner->transport, data)
                              : HAL_QSPI_Receive_IT(&owner->transport, data);
    }
    return status == HAL_OK ? NX_SUCCESS : NX_ERROR_IO;
}
static nx_result_t wire_step(nx_stm32_qspi_t* owner) {
    if (!irq_quiet())
        return NX_ERROR_STATE;
#if NEXUS_QSPI_PROGRAM_DMA
    nx_result_t dma_result = nx_stm32_qspi_dma_service(owner);
    if (dma_result != NX_SUCCESS)
        return dma_result;
    if (owner->config->dma == NULL &&
        (READ_REG(QUADSPI->CR) & QUADSPI_CR_DMAEN) != 0U)
        return NX_ERROR_STATE;
#else
    if ((READ_REG(QUADSPI->CR) & QUADSPI_CR_DMAEN) != 0U)
        return NX_ERROR_STATE;
#endif
    if ((READ_REG(QUADSPI->SR) & QUADSPI_SR_TEF) != 0U)
        owner->transport_error = true;
    if (owner->transport_error) {
        (void)wire_abort(owner);
        return NX_ERROR_IO;
    }
    HAL_QSPI_IRQHandler(&owner->transport);
    if (!owner->complete)
        return NX_ERROR_BUSY;
    if (owner->transport.TxXferCount != 0U ||
        owner->transport.RxXferCount != 0U)
        return NX_ERROR_IO;
    /* HAL publishes its callback before physical ABORT/BUSY exit. */
    return wire_abort(owner);
}
static nx_result_t exchange(nx_stm32_qspi_t* owner, uint8_t instruction,
                            uint32_t address, bool addressed, uint8_t* data,
                            size_t bytes, nx_time_us_t deadline) {
    nx_result_t result = wire_start(owner, instruction, address, addressed,
                                    data, bytes, false, deadline);
    if (result != NX_SUCCESS) {
        if (owner->wire_active) {
            (void)wire_abort(owner);
            forget_buffers(owner);
            owner->prepared = false;
        }
        return result;
    }
    for (;;) {
        nx_time_us_t now;
        result = clock_read(owner, &now);
        if (result != NX_SUCCESS || now >= deadline) {
            result = result != NX_SUCCESS ? result : NX_ERROR_TIMEOUT;
            break;
        }
        result = wire_step(owner);
        if (result != NX_ERROR_BUSY)
            break;
    }
    if (result != NX_SUCCESS) {
        /* Task-only PIO: withdrawing all CPU references ends this read's
         * destination access even if physical cold cleanup remains busy. */
        (void)wire_abort(owner);
        forget_buffers(owner);
        owner->prepared = false;
    }
    return result;
}
static nx_result_t pins_valid(const nx_stm32_qspi_t* owner,
                              const nx_stm32_qspi_config_t* config,
                              bool inspect_hardware) {
    const size_t count = sizeof(pin_facts) / sizeof(pin_facts[0]);
    if (config->pin_count != count ||
        !span(config->pins, sizeof(*config->pins) * count,
              _Alignof(nx_stm32_platform_pin_config_t), false) ||
        overlap(config->pins, sizeof(*config->pins) * count, owner,
                sizeof(*owner)) ||
        overlap(config->pins, sizeof(*config->pins) * count, config->system,
                sizeof(*config->system)))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        GPIO_TypeDef* gpio =
            (GPIO_TypeDef*)(GPIOA_BASE + (uintptr_t)pin_facts[i].port * 0x400U);
        if (pin->port_index != pin_facts[i].port ||
            pin->pin != pin_facts[i].pad || pin->registers != gpio ||
            !span(pin->snapshot, sizeof(uint16_t), _Alignof(uint16_t), true) ||
            overlap(pin->snapshot, sizeof(uint16_t), owner, sizeof(*owner)) ||
            overlap(pin->snapshot, sizeof(uint16_t), config, sizeof(*config)) ||
            overlap(pin->snapshot, sizeof(uint16_t), config->system,
                    sizeof(*config->system)) ||
            overlap(pin->snapshot, sizeof(uint16_t), config->pins,
                    sizeof(*config->pins) * count))
            return NX_ERROR_INVALID;
        for (size_t j = 0U; j < i; ++j)
            if (pin->snapshot == config->pins[j].snapshot)
                return NX_ERROR_INVALID;
        if (!inspect_hardware)
            continue;
        if ((READ_REG(RCC->AHB1ENR) & (1U << pin_facts[i].port)) == 0U)
            return NX_ERROR_STATE;
        uint32_t locked = GPIO_LCKR_LCKK | (1U << pin_facts[i].pad);
        if ((READ_REG(gpio->LCKR) & locked) == locked)
            return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}

/** \brief DMA metadata adds no buffer, register or mutable owner on PIO. */
static nx_result_t dma_config_valid(const nx_stm32_qspi_t* owner,
                                    const nx_stm32_qspi_config_t* config) {
    const nx_stm32_qspi_dma_config_t* dma = config->dma;
    if (dma == NULL)
        return NX_SUCCESS;
#if NEXUS_QSPI_PROGRAM_DMA
    if (!span(dma, sizeof(*dma), _Alignof(nx_stm32_qspi_dma_config_t), false) ||
        overlap(dma, sizeof(*dma), owner, sizeof(*owner)) ||
        overlap(dma, sizeof(*dma), config->system, sizeof(*config->system)) ||
        dma->region_count == 0U || dma->region_count > 8U ||
        !span(dma->regions, dma->region_count * sizeof(*dma->regions),
              _Alignof(nx_dma_memory_region_t), false) ||
        !span(dma->state, sizeof(*dma->state), _Alignof(nx_stm32_qspi_dma_t),
              true))
        return NX_ERROR_INVALID;
    size_t region_bytes = dma->region_count * sizeof(*dma->regions);
    if (overlap(dma->regions, region_bytes, owner, sizeof(*owner)) ||
        overlap(dma->regions, region_bytes, config->system,
                sizeof(*config->system)))
        return NX_ERROR_INVALID;
    const void* facts[] = {owner,        config, config->system,
                           config->pins, dma,    dma->regions};
    const size_t sizes[] = {sizeof(*owner),
                            sizeof(*config),
                            sizeof(*config->system),
                            config->pin_count * sizeof(*config->pins),
                            sizeof(*dma),
                            dma->region_count * sizeof(*dma->regions)};
    for (size_t i = 0U; i < sizeof(facts) / sizeof(facts[0]); ++i)
        if (overlap(dma->state, sizeof(*dma->state), facts[i], sizes[i]))
            return NX_ERROR_INVALID;
    for (size_t i = 0U; i < config->pin_count; ++i)
        if (overlap(dma->state, sizeof(*dma->state), config->pins[i].snapshot,
                    sizeof(uint16_t)) ||
            overlap(dma, sizeof(*dma), config->pins[i].snapshot,
                    sizeof(uint16_t)) ||
            overlap(dma->regions, region_bytes, config->pins[i].snapshot,
                    sizeof(uint16_t)))
            return NX_ERROR_INVALID;
    for (size_t i = 0U; i < dma->region_count; ++i) {
        const nx_dma_memory_region_t* region = &dma->regions[i];
        if (region->size == 0U || region->size > UINTPTR_MAX - region->begin ||
            region->permissions == 0U ||
            (region->permissions &
             ~(uint32_t)(NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE)) != 0U)
            return NX_ERROR_INVALID;
        uintptr_t end = region->begin + region->size;
        if (!((region->begin >= 0x20000000U && end <= 0x20050000U) ||
              (region->begin >= 0x08000000U && end <= 0x08200000U &&
               region->permissions == NX_DMA_MEMORY_READ)))
            return NX_ERROR_PERMISSION;
    }
    return nx_stm32_qspi_dma_admit(dma);
#else
    (void)owner;
    return NX_ERROR_UNSUPPORTED;
#endif
}

nx_result_t nx_stm32_qspi_admit(const nx_stm32_qspi_t* owner,
                                const nx_stm32_qspi_config_t* config) {
    if (!context())
        return NX_ERROR_CONTEXT;
    if (!valid(owner) ||
        !span(config, sizeof(*config), _Alignof(nx_stm32_qspi_config_t),
              false) ||
        overlap(owner, sizeof(*owner), config, sizeof(*config)) ||
        config->system != &g_nx_stm32_system ||
        overlap(owner, sizeof(*owner), config->system,
                sizeof(*config->system)) ||
        overlap(config, sizeof(*config), config->system,
                sizeof(*config->system)) ||
        config->hclk_hz != NEXUS_CORE_HZ || config->cs_high_cycles == 0U ||
        config->cs_high_cycles > 8U || config->read_budget_us == 0U ||
        config->read_budget_us > 1000000U || config->stop_budget_us == 0U ||
        config->prepare_budget_us == 0U ||
        config->prepare_budget_us > 1000000U ||
        config->stop_budget_us > 1000000U ||
        config->hclk_hz > UINT64_C(50000000) * (config->prescaler + 1U) ||
        (uint64_t)config->cs_high_cycles * (config->prescaler + 1U) *
                UINT64_C(1000000000) <
            (uint64_t)config->hclk_hz * 50U)
        return NX_ERROR_INVALID;
    if (owner->config != NULL || owner->request != NULL || owner->gate_added ||
        owner->captured || owner->prepared || owner->wire_active ||
        owner->pulse_uncertain || owner->transport.Instance != NULL ||
        owner->system_epoch != 0U)
        return NX_ERROR_BUSY;
    nx_result_t result = pins_valid(owner, config, false);
    if (result != NX_SUCCESS)
        return result;
#if NEXUS_QSPI_CPU_MAP
    result = nx_stm32_qspi_mapped_admit(owner, config);
    if (result != NX_SUCCESS)
        return result;
#endif
    result = dma_config_valid(owner, config);
    if (result != NX_SUCCESS)
        return result;
    if (!irq_quiet() || NVIC_GetPendingIRQ(QUADSPI_IRQn) != 0U ||
        (READ_REG(RCC->AHB3ENR) & RCC_AHB3ENR_QSPIEN) != 0U)
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
nx_result_t nx_stm32_qspi_prepare(nx_stm32_qspi_t* owner,
                                  const nx_stm32_qspi_config_t* config,
                                  nx_time_us_t deadline) {
    nx_result_t result = nx_stm32_qspi_admit(owner, config);
    if (result != NX_SUCCESS)
        return result;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    result = pins_valid(owner, config, true);
    if (result != NX_SUCCESS)
        return result;
    uint64_t epoch = config->system->time_epoch;
    nx_time_us_t now;
    result = clock_value(config, epoch, &now);
    if (result != NX_SUCCESS || now >= deadline)
        return result != NX_SUCCESS ? result : NX_ERROR_TIMEOUT;
    owner->config = config;
    owner->system_epoch = epoch;
    owner->last_clock = now;
    owner->gate_added = true;
    SET_BIT(RCC->AHB3ENR, RCC_AHB3ENR_QSPIEN);
    nx_arch_dsb();
    if ((READ_REG(RCC->AHB3ENR) & RCC_AHB3ENR_QSPIEN) == 0U)
        return NX_ERROR_IO;
    owner->saved_cr = READ_REG(QUADSPI->CR);
    owner->saved_dcr = READ_REG(QUADSPI->DCR);
    owner->saved_ccr = READ_REG(QUADSPI->CCR);
    owner->saved_dlr = READ_REG(QUADSPI->DLR);
    owner->saved_abr = READ_REG(QUADSPI->ABR);
    owner->captured = true;
    if (owner->saved_cr != 0U || owner->saved_dcr != 0U ||
        owner->saved_ccr != 0U || owner->saved_dlr != 0U ||
        owner->saved_abr != 0U || READ_REG(QUADSPI->SR) != 0U)
        return NX_ERROR_BUSY;
    LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_6);
    LL_GPIO_SetPinOutputType(GPIOB, LL_GPIO_PIN_6, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_6, LL_GPIO_MODE_OUTPUT);
    owner->transport.Instance = QUADSPI;
    owner->transport.Init = (QSPI_InitTypeDef){
        config->prescaler,
        1U,
        QSPI_SAMPLE_SHIFTING_NONE,
        23U,
        (uint32_t)(config->cs_high_cycles - 1U) << QUADSPI_DCR_CSHT_Pos,
        QSPI_CLOCK_MODE_0,
        QSPI_FLASH_ID_1,
        QSPI_DUALFLASH_DISABLE};
    /* Platform/C owns every gate/pin. A legacy global MSP hook cannot acquire
     * or release a second set of resources behind this per-handle owner. */
    if (HAL_QSPI_RegisterCallback(&owner->transport, HAL_QSPI_MSP_INIT_CB_ID,
                                  noop) != HAL_OK ||
        HAL_QSPI_RegisterCallback(&owner->transport, HAL_QSPI_MSP_DEINIT_CB_ID,
                                  noop) != HAL_OK)
        return NX_ERROR_IO;
    if (HAL_QSPI_Init(&owner->transport) != HAL_OK)
        return NX_ERROR_IO;
    /* Quiescent preflight avoids HAL's tick wait. Later Command_IT uses a
     * nonzero, bounded CPU-cycle guard; normal admission proves BUSY0. */
    HAL_QSPI_SetTimeout(&owner->transport, 1U);
    static const HAL_QSPI_CallbackIDTypeDef callbacks[] = {
        HAL_QSPI_CMD_CPLT_CB_ID, HAL_QSPI_RX_CPLT_CB_ID, HAL_QSPI_TX_CPLT_CB_ID,
        HAL_QSPI_ABORT_CB_ID};
    for (size_t i = 0U; i < sizeof(callbacks) / sizeof(callbacks[0]); ++i)
        if (HAL_QSPI_RegisterCallback(&owner->transport, callbacks[i],
                                      complete) != HAL_OK)
            return NX_ERROR_IO;
    if (HAL_QSPI_RegisterCallback(&owner->transport, HAL_QSPI_ERROR_CB_ID,
                                  transport_error) != HAL_OK)
        return NX_ERROR_IO;
#if NEXUS_QSPI_PROGRAM_DMA
    result = nx_stm32_qspi_dma_prepare(owner);
    if (result != NX_SUCCESS)
        return result;
#endif
    static const HAL_QSPI_CallbackIDTypeDef noops[] = {
        HAL_QSPI_FIFO_THRESHOLD_CB_ID, HAL_QSPI_RX_HALF_CPLT_CB_ID,
        HAL_QSPI_TX_HALF_CPLT_CB_ID, HAL_QSPI_STATUS_MATCH_CB_ID,
        HAL_QSPI_TIMEOUT_CB_ID};
    for (size_t i = 0U; i < sizeof(noops) / sizeof(noops[0]); ++i)
        if (HAL_QSPI_RegisterCallback(&owner->transport, noops[i], noop) !=
            HAL_OK)
            return NX_ERROR_IO;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        uint32_t mask = 1U << pin->pin;
        uint32_t af = pin_facts[i].af;
        LL_GPIO_SetPinOutputType(pin->registers, mask, LL_GPIO_OUTPUT_PUSHPULL);
        LL_GPIO_SetPinPull(pin->registers, mask, LL_GPIO_PULL_NO);
        LL_GPIO_SetPinSpeed(pin->registers, mask, LL_GPIO_SPEED_FREQ_VERY_HIGH);
        if (pin->pin < 8U)
            LL_GPIO_SetAFPin_0_7(pin->registers, mask, af);
        else
            LL_GPIO_SetAFPin_8_15(pin->registers, mask, af);
        LL_GPIO_SetPinMode(pin->registers, mask, LL_GPIO_MODE_ALTERNATE);
    }
    nx_arch_dsb();
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        uint32_t shift = (uint32_t)pin->pin * 2U;
        uint32_t af = pin_facts[i].af;
        if (((READ_REG(pin->registers->MODER) >> shift) & 3U) !=
                LL_GPIO_MODE_ALTERNATE ||
            ((READ_REG(pin->registers->OTYPER) >> pin->pin) & 1U) != 0U ||
            ((READ_REG(pin->registers->PUPDR) >> shift) & 3U) != 0U ||
            ((READ_REG(pin->registers->OSPEEDR) >> shift) & 3U) !=
                LL_GPIO_SPEED_FREQ_VERY_HIGH ||
            ((READ_REG(pin->registers->AFR[pin->pin / 8U]) >>
              ((uint32_t)pin->pin % 8U * 4U)) &
             15U) != af)
            return NX_ERROR_IO;
    }
    uint32_t cr = QUADSPI_CR_EN | (uint32_t)config->prescaler
                                      << QUADSPI_CR_PRESCALER_Pos;
    uint32_t dcr =
        23U << QUADSPI_DCR_FSIZE_Pos | (uint32_t)(config->cs_high_cycles - 1U)
                                           << QUADSPI_DCR_CSHT_Pos;
    if (READ_REG(QUADSPI->CR) != cr || READ_REG(QUADSPI->DCR) != dcr ||
        !hardware_idle())
        return NX_ERROR_IO;
    uint8_t id[3];
    result = exchange(owner, 0x9fU, 0U, false, id, sizeof(id), deadline);
    if (result != NX_SUCCESS)
        return result;
    if (id[0] != 0xefU || id[1] != 0x40U || id[2] != 0x18U)
        return NX_ERROR_UNSUPPORTED;
#if NEXUS_QSPI_QUAD_READ
    uint8_t status2;
    result =
        exchange(owner, 0x35U, 0U, false, &status2, sizeof(status2), deadline);
    if (result != NX_SUCCESS)
        return result;
    /* Winbond RevM 7.1.4: SIQ fixes QE1. The JEDEC ID alone cannot prove
     * that suffix. Adopt QE1/SUS0 without changing status, protection or a
     * foreign suspended operation; no implicit resume or reset is permitted. */
    if ((status2 & 0x82U) != 2U)
        return NX_ERROR_UNSUPPORTED;
#endif
    owner->prepared = true;
    owner->async_stopped = false;
#if NEXUS_QSPI_CPU_MAP
    return nx_stm32_qspi_mapped_prepare(owner, config->mapping);
#else
    return NX_SUCCESS;
#endif
}
static const nx_flash_geometry_t* geometry(const void* selected) {
    const nx_stm32_qspi_t* owner = selected;
    return valid(owner) && owner->config != NULL && owner->prepared &&
                   epoch_live(owner)
               ? &physical
               : NULL;
}
static nx_result_t read_flash(const void* selected, uint32_t offset, void* data,
                              size_t length) {
    nx_stm32_qspi_t* owner = (nx_stm32_qspi_t*)selected;
#if NEXUS_QSPI_CPU_MAP
    (void)owner;
    (void)offset;
    (void)data;
    (void)length;
    return NX_ERROR_UNSUPPORTED;
#endif
    if (!context())
        return NX_ERROR_CONTEXT;
    if (!valid(owner) || owner->config == NULL || !owner->prepared ||
        !epoch_live(owner))
        return NX_ERROR_STATE;
    if (owner->request != NULL || owner->wire_active || owner->pulse_uncertain)
        return NX_ERROR_BUSY;
    if (offset > physical.size || length > physical.size - offset ||
        (length != 0U && !isolated(owner, data, length, 1U, true)))
        return NX_ERROR_INVALID;
    if (length == 0U)
        return NX_SUCCESS;
    nx_time_us_t now, deadline;
    nx_result_t result = clock_read(owner, &now);
    if (result == NX_SUCCESS)
        result = nx_deadline_finite_after(now, owner->config->read_budget_us,
                                          &deadline);
    if (result != NX_SUCCESS)
        return result;
    for (size_t done = 0U; done < length;) {
        size_t count = length - done;
        if (count > 32U)
            count = 32U;
        result = exchange(owner, read_instruction, offset + (uint32_t)done,
                          true, (uint8_t*)data + done, count, deadline);
        if (result != NX_SUCCESS)
            return result;
        done += count;
    }
    return NX_SUCCESS;
}
static void terminal(nx_stm32_qspi_t* owner, nx_result_t result) {
    if (owner->terminal == NX_SUCCESS)
        owner->terminal = result;
    if (!owner->draining) {
        owner->draining = true;
        (void)nx_request_transition(&owner->request->base, NX_REQUEST_DRAINING);
    }
}
static void quarantine(nx_stm32_qspi_t* owner) {
    (void)nx_request_transition(&owner->request->base, NX_REQUEST_QUARANTINED);
}
static void settle(nx_stm32_qspi_t* owner) {
    nx_flash_async_request_t* request = owner->request;
    size_t completed = owner->completed;
    nx_result_t result = owner->terminal;
    owner->request = NULL;
    owner->phase = IDLE;
    owner->draining = false;
    owner->chunk = owner->verified = owner->completed = 0U;
    nx_request_settle(&request->base, result, completed);
}
static size_t verify_bytes(const nx_stm32_qspi_t* owner) {
    size_t bytes = owner->chunk - owner->verified;
    return bytes > sizeof(owner->verify) ? sizeof(owner->verify) : bytes;
}
static nx_result_t begin_phase(nx_stm32_qspi_t* owner) {
    nx_flash_async_request_t* request = owner->request;
    nx_time_us_t deadline = request->base.deadline;
    if (owner->draining) {
        nx_time_us_t now;
        nx_result_t result = clock_read(owner, &now);
        if (result == NX_SUCCESS)
            result = nx_deadline_finite_after(
                now, owner->config->stop_budget_us, &deadline);
        if (result != NX_SUCCESS)
            return result;
    }
    uint8_t instruction = 0U;
    uint32_t address = 0U;
    bool addressed = false, transmit = false;
    uint8_t* data = &owner->status;
    size_t bytes = 1U;
    switch (owner->phase) {
        case STATUS1:
        case CHECK_WEL:
        case POLL_READY:
            instruction = 0x05U;
            break;
        case STATUS2:
            instruction = 0x35U;
            break;
        case STATUS3:
            instruction = 0x15U;
            break;
        case ENABLE_WRITE:
            instruction = 0x06U;
            bytes = 0U;
            break;
        case DISABLE_WRITE:
            instruction = 0x04U;
            bytes = 0U;
            break;
        case PULSE:
            instruction =
                request->operation == NX_FLASH_ASYNC_PROGRAM ? 0x02U : 0x20U;
            address = request->offset + (uint32_t)owner->completed;
            addressed = true;
            transmit = request->operation == NX_FLASH_ASYNC_PROGRAM;
            data = transmit
                       ? (uint8_t*)(uintptr_t)request->data + owner->completed
                       : NULL;
            bytes = transmit ? owner->chunk : 0U;
            owner->pulse_uncertain =
                true; /* Never infer no pulse after a failed frame. */
            break;
        case VERIFY:
            instruction = 0x03U;
            addressed = true;
            address = request->offset +
                      (uint32_t)(owner->completed + owner->verified);
            data = owner->verify;
            bytes = verify_bytes(owner);
            break;
        default:
            return NX_ERROR_STATE;
    }
    return wire_start(owner, instruction, address, addressed, data, bytes,
                      transmit, deadline);
}
static nx_result_t service(void* selected) {
    nx_stm32_qspi_t* owner = selected;
    if (!context())
        return NX_ERROR_CONTEXT;
    if (!valid(owner) || owner->config == NULL)
        return NX_ERROR_STATE;
    if (owner->request == NULL)
        return NX_SUCCESS;
    nx_time_us_t now;
    nx_result_t result = clock_read(owner, &now);
    if (result != NX_SUCCESS) {
        terminal(owner, result);
        (void)quiet_sources(owner);
        owner->phase = DRAIN_WIRE;
        (void)nx_request_transition(&owner->request->base,
                                    NX_REQUEST_QUARANTINED);
        return result;
    }
    if (now >= owner->request->base.deadline)
        terminal(owner, NX_ERROR_TIMEOUT);
    if (owner->draining && owner->phase != POLL_READY &&
        owner->phase != VERIFY && owner->phase != DISABLE_WRITE) {
        result = wire_abort(owner);
        if (result != NX_SUCCESS) {
            if (result != NX_ERROR_BUSY)
                quarantine(owner);
            return result;
        }
        owner->complete = false;
        owner->phase = POLL_READY;
    }
    if (owner->wire_active) {
        result = wire_step(owner);
        if (result == NX_ERROR_BUSY)
            return result;
        if (result != NX_SUCCESS) {
            terminal(owner, result);
            /* A failed join has no permission to start the next status
             * frame. Retry physical withdrawal before polling Flash WIP. */
            owner->phase = DRAIN_WIRE;
            owner->complete = false;
            quarantine(owner);
            return result;
        }
    }
    if (!owner->complete) {
        result = begin_phase(owner);
        if (result != NX_SUCCESS && result != NX_ERROR_BUSY) {
            terminal(owner, result);
            owner->phase = DRAIN_WIRE;
            if (result != NX_ERROR_TIMEOUT)
                quarantine(owner);
        }
        return result;
    }
    owner->complete = false;
    switch (owner->phase) {
        case STATUS1:
            if ((owner->status & 1U) != 0U)
                return NX_ERROR_BUSY;
            if ((owner->status & 0x1cU) != 0U) {
                terminal(owner, NX_ERROR_UNSUPPORTED);
                owner->phase = POLL_READY;
            } else {
                owner->phase = STATUS2;
            }
            break;
        case STATUS2:
            if ((owner->status & 0x40U) != 0U) {
                terminal(owner, NX_ERROR_UNSUPPORTED);
                owner->phase = POLL_READY;
            } else {
                owner->phase = STATUS3;
            }
            break;
        case STATUS3:
            if ((owner->status & 4U) != 0U) {
                terminal(owner, NX_ERROR_UNSUPPORTED);
                owner->phase = POLL_READY;
            } else {
                size_t left = owner->request->length - owner->completed;
                owner->chunk =
                    owner->request->operation == NX_FLASH_ASYNC_ERASE
                        ? 4096U
                        : 256U - ((owner->request->offset + owner->completed) %
                                  256U);
                if (owner->chunk > left)
                    owner->chunk = left;
                owner->phase = ENABLE_WRITE;
                if (owner->request->operation == NX_FLASH_ASYNC_PROGRAM) {
                    const uint8_t* input = owner->request->data;
                    bool erased = true;
                    for (size_t i = 0U; i < owner->chunk; ++i)
                        if (input[owner->completed + i] != 0xffU) {
                            erased = false;
                            break;
                        }
                    if (erased) {
                        owner->verified = 0U;
                        owner->phase = VERIFY;
                    }
                }
            }
            break;
        case ENABLE_WRITE:
            owner->phase = CHECK_WEL;
            break;
        case CHECK_WEL:
            if ((owner->status & 3U) != 2U) {
                terminal(owner, NX_ERROR_IO);
                owner->phase = POLL_READY;
            } else {
                owner->phase = PULSE;
            }
            break;
        case PULSE:
            owner->phase = POLL_READY;
            break;
        case POLL_READY:
            if ((owner->status & 1U) != 0U)
                return NX_ERROR_BUSY;
            owner->pulse_uncertain = false;
            if ((owner->status & 2U) != 0U) {
                owner->phase = DISABLE_WRITE;
            } else if (owner->chunk != 0U) {
                owner->verified = 0U;
                owner->phase = VERIFY;
            } else if (owner->draining) {
                settle(owner);
                return NX_SUCCESS;
            } else {
                return NX_ERROR_STATE;
            }
            break;
        case DISABLE_WRITE:
            owner->phase = POLL_READY;
            break;
        case VERIFY: {
            size_t bytes = verify_bytes(owner);
            const uint8_t* input = owner->request->data;
            for (size_t i = 0U; i < bytes; ++i) {
                uint8_t expected =
                    owner->request->operation == NX_FLASH_ASYNC_ERASE
                        ? 0xffU
                        : input[owner->completed + owner->verified + i];
                if (owner->verify[i] != expected) {
                    terminal(owner, NX_ERROR_IO);
                    owner->chunk = 0U;
                    settle(owner);
                    return NX_ERROR_IO;
                }
            }
            owner->verified += bytes;
            if (owner->verified == owner->chunk) {
                owner->completed += owner->chunk;
                owner->chunk = owner->verified = 0U;
                if (owner->draining ||
                    owner->completed == owner->request->length) {
                    settle(owner);
                    return NX_SUCCESS;
                }
                owner->phase = STATUS1;
            }
            break;
        }
        default:
            return NX_ERROR_STATE;
    }
    return NX_SUCCESS;
}
static nx_result_t submit(void* selected, nx_flash_async_request_t* request) {
    nx_stm32_qspi_t* owner = selected;
#if NEXUS_QSPI_CPU_MAP
    if (valid(owner) && request != &((nx_stm32_qspi_mapped_t*)owner)->inner)
        return NX_ERROR_PERMISSION;
#endif
    if (!context())
        return NX_ERROR_CONTEXT;
    if (!valid(owner) || owner->config == NULL || !owner->prepared ||
        !epoch_live(owner))
        return NX_ERROR_STATE;
    if (owner->async_stopped)
        return NX_ERROR_STATE;
    if ((READ_REG(RCC->AHB3ENR) & RCC_AHB3ENR_QSPIEN) == 0U)
        return NX_ERROR_STATE;
    if (owner->request != NULL || owner->wire_active ||
        owner->pulse_uncertain || !hardware_idle())
        return NX_ERROR_BUSY;
    if (!isolated(owner, request, sizeof(*request),
                  _Alignof(nx_flash_async_request_t), true) ||
        request->length == 0U || request->offset >= physical.size ||
        request->length > physical.size - request->offset ||
        request->base.deadline == NX_DEADLINE_NEVER ||
        (request->operation != NX_FLASH_ASYNC_PROGRAM &&
         request->operation != NX_FLASH_ASYNC_ERASE) ||
        (request->operation == NX_FLASH_ASYNC_ERASE
             ? request->data != NULL || request->offset % 4096U != 0U ||
                   request->length % 4096U != 0U
             : !isolated(owner, request->data, request->length, 1U, false) ||
                   overlap(request, sizeof(*request), request->data,
                           request->length)))
        return NX_ERROR_INVALID;
    nx_time_us_t now;
    nx_result_t result = clock_read(owner, &now);
    if (result != NX_SUCCESS || now >= request->base.deadline)
        return result != NX_SUCCESS ? result : NX_ERROR_TIMEOUT;
#if NEXUS_QSPI_PROGRAM_DMA
    if (request->operation == NX_FLASH_ASYNC_PROGRAM &&
        owner->config->dma != NULL) {
        const nx_stm32_qspi_dma_config_t* dma = owner->config->dma;
        result = nx_dma_buffer_validate(dma->regions, dma->region_count,
                                        request->data, request->length, 1U,
                                        NX_DMA_TO_DEVICE);
        if (result != NX_SUCCESS)
            return result;
    }
#endif
    result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS)
        return result;
    owner->request = request;
    owner->completed = owner->chunk = owner->verified = 0U;
    owner->terminal = NX_SUCCESS;
    owner->complete = owner->draining = false;
    owner->phase = STATUS1;
    return NX_SUCCESS;
}
static nx_result_t cancel(void* selected, nx_flash_async_request_t* request) {
    nx_stm32_qspi_t* owner = selected;
    if (!context())
        return NX_ERROR_CONTEXT;
    if (!valid(owner) || owner->request != request)
        return NX_ERROR_STATE;
    terminal(owner, NX_ERROR_CANCELLED);
    return NX_SUCCESS;
}
static nx_result_t recover(void* selected) {
    nx_stm32_qspi_t* owner = selected;
    if (!context())
        return NX_ERROR_CONTEXT;
    if (!valid(owner) || owner->config == NULL)
        return NX_ERROR_STATE;
    if (owner->request != NULL) {
        if (!owner->draining)
            return NX_ERROR_BUSY;
        nx_result_t result = service(owner);
        return owner->request == NULL ? NX_SUCCESS
               : result == NX_SUCCESS ? NX_ERROR_BUSY
                                      : result;
    }
    nx_time_us_t now;
    nx_result_t result = clock_read(owner, &now);
    return result == NX_SUCCESS ? wire_abort(owner) : result;
}
/** \brief Retire the owned gate only after its relevant hardware proof. */
static nx_result_t release_gate(nx_stm32_qspi_t* owner) {
#if NEXUS_QSPI_PROGRAM_DMA
    nx_result_t result = nx_stm32_qspi_dma_stop(owner);
    if (result != NX_SUCCESS)
        return result;
#endif
    CLEAR_BIT(RCC->AHB3ENR, RCC_AHB3ENR_QSPIEN);
    nx_arch_dsb();
    if ((READ_REG(RCC->AHB3ENR) & RCC_AHB3ENR_QSPIEN) != 0U)
        return NX_ERROR_IO;
    owner->config = NULL;
    owner->system_epoch = 0U;
    owner->last_clock = 0U;
    owner->captured = owner->gate_added = false;
    owner->async_stopped = false;
    owner->transport.Instance = NULL;
    owner->transport.State = HAL_QSPI_STATE_RESET;
    owner->complete = owner->transport_error = owner->wire_active = false;
    return NX_SUCCESS;
}
nx_result_t nx_stm32_qspi_stop(nx_stm32_qspi_t* owner) {
    if (!context())
        return NX_ERROR_CONTEXT;
    if (!valid(owner))
        return NX_ERROR_INVALID;
    if (owner->config == NULL)
        return NX_SUCCESS;
#if NEXUS_QSPI_CPU_MAP
    nx_result_t mapped = nx_stm32_qspi_mapped_stop(owner);
    if (mapped != NX_SUCCESS)
        return mapped;
#endif
    owner->prepared = false;
    owner->async_stopped = true;
    /* Instance is bound only after reset-like observation, before our first
     * controller configuration. Before that point no HAL abort/clear is ours.
     * The sole effect is the gate; retain it while foreign hardware is busy. */
    if (owner->transport.Instance == NULL) {
        if ((READ_REG(RCC->AHB3ENR) & RCC_AHB3ENR_QSPIEN) != 0U) {
            if ((READ_REG(QUADSPI->SR) &
                 (QUADSPI_SR_BUSY | QUADSPI_SR_FLEVEL)) != 0U)
                return NX_ERROR_BUSY;
        }
        return release_gate(owner);
    }
    if ((READ_REG(RCC->AHB3ENR) & RCC_AHB3ENR_QSPIEN) == 0U)
        return NX_ERROR_STATE;
    if (owner->request != NULL) {
        terminal(owner, NX_ERROR_CANCELLED);
        (void)service(owner);
        if (owner->request != NULL)
            return NX_ERROR_BUSY;
    }
    nx_result_t result = recover(owner);
    if (result != NX_SUCCESS || owner->pulse_uncertain)
        return result != NX_SUCCESS ? result : NX_ERROR_BUSY;
    /* Disable before restoring empty communication fields: no restore write
     * is allowed to start another external instruction or chip selection. */
    __HAL_QSPI_DISABLE(&owner->transport);
    nx_arch_dsb();
    if ((READ_REG(QUADSPI->CR) & QUADSPI_CR_EN) != 0U || !hardware_idle())
        return NX_ERROR_IO;
    WRITE_REG(QUADSPI->CCR, owner->saved_ccr);
    WRITE_REG(QUADSPI->DLR, owner->saved_dlr);
    WRITE_REG(QUADSPI->ABR, owner->saved_abr);
    WRITE_REG(QUADSPI->DCR, owner->saved_dcr);
    WRITE_REG(QUADSPI->CR, owner->saved_cr);
    nx_arch_dsb();
    if (READ_REG(QUADSPI->CCR) != owner->saved_ccr ||
        READ_REG(QUADSPI->DLR) != owner->saved_dlr ||
        READ_REG(QUADSPI->ABR) != owner->saved_abr ||
        READ_REG(QUADSPI->DCR) != owner->saved_dcr ||
        READ_REG(QUADSPI->CR) != owner->saved_cr || !hardware_idle())
        return NX_ERROR_IO;
    return release_gate(owner);
}
static nx_result_t stop(void* selected) {
    return nx_stm32_qspi_stop(selected);
}
/** \brief Seal the endpoint without stealing the Platform cold-close role. */
static nx_result_t stop_io(void* selected) {
    nx_stm32_qspi_t* owner = selected;
    if (!context())
        return NX_ERROR_CONTEXT;
    if (!valid(owner))
        return NX_ERROR_INVALID;
    owner->async_stopped = true;
    if (owner->config == NULL)
        return NX_SUCCESS;
    if (owner->request != NULL) {
        terminal(owner, NX_ERROR_CANCELLED);
        nx_result_t result = service(owner);
        if (owner->request != NULL)
            return result == NX_SUCCESS ? NX_ERROR_BUSY : result;
    }
    if (owner->pulse_uncertain)
        return NX_ERROR_BUSY;
    return owner->wire_active ? recover(owner) : NX_SUCCESS;
}
const nx_flash_ops_t nx_stm32_qspi_flash_ops = {geometry, read_flash, NULL,
                                                NULL, recover};
const nx_flash_async_ops_t nx_stm32_qspi_async_ops = {submit, cancel, service,
                                                      stop_io};
nx_result_t nx_stm32_qspi_start(nx_stm32_qspi_t* owner,
                                const nx_stm32_qspi_config_t* config) {
    nx_result_t result = nx_stm32_qspi_admit(owner, config);
    if (result != NX_SUCCESS)
        return result;
    uint64_t epoch = config->system->time_epoch;
    nx_time_us_t now, deadline;
    result = clock_value(config, epoch, &now);
    if (result == NX_SUCCESS)
        result =
            nx_deadline_finite_after(now, config->prepare_budget_us, &deadline);
    return result == NX_SUCCESS ? nx_stm32_qspi_prepare(owner, config, deadline)
                                : result;
}
static nx_external_bus_state_t bus_state(const void* selected) {
    const nx_stm32_qspi_t* owner = selected;
    if (!valid(owner))
        return NX_EXTERNAL_BUS_UNAVAILABLE;
    return owner->config == NULL ? NX_EXTERNAL_BUS_STOPPED
           : owner->prepared && epoch_live(owner)
               ? NX_EXTERNAL_BUS_PREPARED
               : NX_EXTERNAL_BUS_CLEANUP_PENDING;
}
const nx_external_bus_ops_t nx_stm32_qspi_bus_ops = {bus_state, stop};

#if NEXUS_QSPI_CPU_MAP
nx_result_t nx_stm32_qspi_mapping_ready(const nx_stm32_qspi_t* owner) {
    if (!context())
        return NX_ERROR_CONTEXT;
    if (!span(owner, sizeof(nx_stm32_qspi_mapped_t),
              _Alignof(nx_stm32_qspi_mapped_t), true))
        return NX_ERROR_INVALID;
    return owner->config != NULL && owner->prepared && epoch_live(owner)
               ? NX_SUCCESS
               : NX_ERROR_STATE;
}
bool nx_stm32_qspi_mapping_external(const nx_stm32_qspi_t* owner,
                                    const void* object, size_t bytes,
                                    bool write) {
    return bytes != 0U && isolated(owner, object, bytes, 1U, write) &&
           !overlap(object, bytes, owner->config->mapping,
                    sizeof(*owner->config->mapping));
}
/** \brief Explicit bounded task cleanup; BUSY never returns a borrow. */
static nx_result_t mapping_exit(nx_stm32_qspi_t* owner) {
    nx_time_us_t now, deadline;
    nx_result_t result = clock_read(owner, &now);
    if (result != NX_SUCCESS)
        return result;
    result =
        nx_deadline_finite_after(now, owner->config->stop_budget_us, &deadline);
    if (result != NX_SUCCESS)
        return result;
    for (unsigned work = 0U; work < 256U; ++work) {
        result = wire_abort(owner);
        if (result != NX_ERROR_BUSY)
            break;
        if (clock_read(owner, &now) != NX_SUCCESS || now >= deadline)
            return NX_ERROR_BUSY;
    }
    if (result != NX_SUCCESS)
        return result;
    /* HAL abort's BUSY0 shortcut leaves FMODE11 after an unused map. */
    CLEAR_BIT(QUADSPI->CCR, QUADSPI_CCR_FMODE);
    CLEAR_BIT(QUADSPI->CR, QUADSPI_CR_TCEN);
    nx_arch_dsb();
    return hardware_idle() &&
                   (READ_REG(QUADSPI->CCR) & QUADSPI_CCR_FMODE) == 0U &&
                   (READ_REG(QUADSPI->CR) &
                    (QUADSPI_CR_TCEN | interrupt_bits)) == 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
nx_result_t nx_stm32_qspi_mapping_leave(nx_stm32_qspi_t* owner) {
    /* Every reader has joined before this same-CPU completion barrier. */
    nx_arch_dsb();
    return mapping_exit(owner);
}
nx_mapped_storage_map_result_t
nx_stm32_qspi_mapping_enter(nx_stm32_qspi_t* owner) {
    nx_result_t result = nx_stm32_qspi_mapping_ready(owner);
    if (result != NX_SUCCESS || owner->request != NULL || owner->wire_active ||
        owner->pulse_uncertain || !hardware_idle())
        return (nx_mapped_storage_map_result_t){
            result != NX_SUCCESS ? result : NX_ERROR_BUSY, false};
    nx_time_us_t now, deadline;
    result = clock_read(owner, &now);
    if (result == NX_SUCCESS)
        result = nx_deadline_finite_after(now, owner->config->prepare_budget_us,
                                          &deadline);
    if (result != NX_SUCCESS)
        return (nx_mapped_storage_map_result_t){result, false};
    uint8_t status = 0U;
    result = exchange(owner, 0x05U, 0U, false, &status, 1U, deadline);
    if (result != NX_SUCCESS || (status & 1U) != 0U)
        return (nx_mapped_storage_map_result_t){
            result != NX_SUCCESS ? result : NX_ERROR_BUSY, owner->wire_active};
    /* ES0321 2.4.3: no DR write after AR0; a read-mode AR write starts work. */
    uint32_t previous = READ_REG(QUADSPI->CCR) & QUADSPI_CCR_FMODE;
    WRITE_REG(QUADSPI->AR, 0U);
    owner->wire_active = true;
    if (previous == QUADSPI_CCR_FMODE_0) {
        SET_BIT(QUADSPI->CR, QUADSPI_CR_ABORT);
        nx_arch_dsb();
    }
    result = mapping_exit(owner);
    if (result != NX_SUCCESS || READ_REG(QUADSPI->AR) != 0U)
        return (nx_mapped_storage_map_result_t){
            result != NX_SUCCESS ? result : NX_ERROR_IO, true};
    QSPI_CommandTypeDef command = {0};
    command.Instruction = 0x6bU;
    command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command.AddressMode = QSPI_ADDRESS_1_LINE;
    command.AddressSize = QSPI_ADDRESS_24_BITS;
    command.DataMode = QSPI_DATA_4_LINES;
    command.DummyCycles = 8U;
    command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    QSPI_MemoryMappedTypeDef memory = {QSPI_TIMEOUT_COUNTER_DISABLE, 0U};
    uint32_t timeout = owner->transport.Timeout;
    owner->transport.Timeout =
        0U; /* Preflight-idle, single bounded HAL check. */
    HAL_StatusTypeDef status_hal =
        HAL_QSPI_MemoryMapped(&owner->transport, &command, &memory);
    owner->transport.Timeout = timeout;
    uint32_t ccr = 0x6bU | QSPI_INSTRUCTION_1_LINE | QSPI_ADDRESS_1_LINE |
                   QSPI_ADDRESS_24_BITS | QSPI_DATA_4_LINES |
                   (8U << QUADSPI_CCR_DCYC_Pos) | QUADSPI_CCR_FMODE;
    nx_arch_dsb();
    bool good = status_hal == HAL_OK && READ_REG(QUADSPI->CCR) == ccr &&
                (READ_REG(QUADSPI->CR) &
                 (QUADSPI_CR_TCEN | interrupt_bits | QUADSPI_CR_DMAEN)) == 0U;
    return (nx_mapped_storage_map_result_t){good ? NX_SUCCESS : NX_ERROR_IO,
                                            true};
}
#endif
