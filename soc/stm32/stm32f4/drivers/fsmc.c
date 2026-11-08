/**
 * \file            fsmc.c
 * \brief           Exact shared FSMC bank ownership and settled LCD transfers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_fsmc.h"
#include "stm32f4xx_ll_fsmc.h"
#include "stm32f4xx_ll_gpio.h"

#ifndef NX_STM32_FSMC_READ
#define NX_STM32_FSMC_READ(reg) (reg)
#endif
#ifndef NX_STM32_FSMC_READ16
#define NX_STM32_FSMC_READ16(address) (*(address))
#endif
#ifndef NX_STM32_FSMC_WRITE16
#define NX_STM32_FSMC_WRITE16(address, value) (*(address) = (value))
#endif

#define NX_FSMC_SRAM_ADDRESS UINT32_C(0x68000000)
#define NX_FSMC_SRAM_BYTES   UINT32_C(0x100000)
#define NX_FSMC_LCD_COMMAND  UINT32_C(0x6c000000)
#define NX_FSMC_LCD_DATA     UINT32_C(0x6c002000)
#define NX_FSMC_RESET_BANK1  UINT32_C(0x30db)

/** \brief Constructors and resource release run only in cold Thread mode. */
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief Validate a complete object span before range arithmetic. */
static bool valid_span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && address % alignment == 0U &&
           address <= UINTPTR_MAX - bytes;
}

/** \brief Classify only known physical ranges, not arbitrary caller memory. */
static bool physical_overlap(const void* pointer, size_t bytes, uint64_t first,
                             uint64_t last) {
    uint64_t address = (uint64_t)(uintptr_t)pointer;
    return address < last && first < address + (uint64_t)bytes;
}

/** \brief Cold facts/control cannot live in peripheral or external apertures.
 */
static bool fact_storage(const void* pointer, size_t bytes) {
    return !physical_overlap(pointer, bytes, UINT64_C(0x40000000),
                             UINT64_C(0x100000000));
}

/** \brief Mutable owner/progress storage is internal, never physical Flash. */
static bool control_storage(const void* pointer, size_t bytes) {
    return fact_storage(pointer, bytes) &&
           !physical_overlap(pointer, bytes, UINT64_C(0x08000000),
                             UINT64_C(0x10000000));
}

/** \brief Caller has already validated both complete spans. */
static bool overlap(const void* first, size_t first_bytes, const void* second,
                    size_t second_bytes) {
    return (uintptr_t)first < (uintptr_t)second + second_bytes &&
           (uintptr_t)second < (uintptr_t)first + first_bytes;
}

/** \brief An opaque context anchor has identity, without an inferred extent. */
static bool contains_anchor(const void* object, size_t bytes,
                            const void* anchor) {
    return anchor != NULL && (uintptr_t)anchor >= (uintptr_t)object &&
           (uintptr_t)anchor - (uintptr_t)object < bytes;
}

/** \brief Mutable borrower objects cannot overlap facts or live receipts. */
static bool outside_configuration(const nx_stm32_fsmc_t* bus,
                                  const nx_stm32_fsmc_config_t* config,
                                  const nx_stm32_fsmc_bank_state_t* banks,
                                  const void* object, size_t bytes) {
    return !overlap(bus, sizeof(*bus), object, bytes) &&
           !overlap(config, sizeof(*config), object, bytes) &&
           !overlap(banks, config->bank_count * sizeof(*banks), object,
                    bytes) &&
           !overlap(config->banks, config->bank_count * sizeof(*config->banks),
                    object, bytes) &&
           !overlap(config->pins, config->pin_count * sizeof(*config->pins),
                    object, bytes) &&
           !contains_anchor(object, bytes, config->clock.context);
}

/** \brief Exact private asynchronous personalities never alias another bank. */
static uint32_t bank_index(nx_stm32_fsmc_kind_t kind) {
    return kind == NX_STM32_FSMC_SRAM_NE3 ? FSMC_NORSRAM_BANK3
                                          : FSMC_NORSRAM_BANK4;
}

/** \brief Reject illegal encoded fields before the official LL sees them. */
static bool timing_valid(const nx_stm32_fsmc_timing_t* timing) {
    return timing->address_setup <= 15U && timing->address_hold >= 1U &&
           timing->address_hold <= 15U && timing->data_setup >= 1U &&
           timing->bus_turn <= 15U;
}

/** \brief Only selected banks require the common address/data route. */
static void required_pins(const nx_stm32_fsmc_config_t* config,
                          uint16_t masks[4]) {
    masks[0] = UINT16_C(0xc733);
    masks[1] = UINT16_C(0xff80);
    masks[2] = 0U;
    masks[3] = 0U;
    for (size_t i = 0U; i < config->bank_count; ++i) {
        if (config->banks[i].kind == NX_STM32_FSMC_SRAM_NE3) {
            masks[0] |= UINT16_C(0x3800);
            masks[1] |= UINT16_C(0x0003);
            masks[2] |= UINT16_C(0xf03f);
            masks[3] |= UINT16_C(0x043f);
        } else {
            masks[3] |= UINT16_C(0x1004);
        }
    }
}

/** \brief Exact reset-owned SRAM pads never have a runtime restore snapshot. */
static bool boot_pin(const nx_stm32_platform_pin_config_t* pin) {
    if (pin->port_index < 3U || pin->port_index > 6U || pin->pin > 15U)
        return false;
    uint16_t mask;
    switch (pin->port_index) {
        case 3U:
            mask = UINT16_C(0xff33);
            break;
        case 4U:
            mask = UINT16_C(0xff83);
            break;
        case 5U:
            mask = UINT16_C(0xf03f);
            break;
        default:
            mask = UINT16_C(0x043f);
            break;
    }
    return (mask & (uint16_t)(1U << pin->pin)) != 0U;
}

/** \brief Each admitted pad and its Platform snapshot occur exactly once. */
static nx_result_t pins_valid(const nx_stm32_fsmc_config_t* config,
                              bool adopting_boot) {
    uint16_t remaining[4];
    required_pins(config, remaining);
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        if (pin->port_index < 3U || pin->port_index > 6U || pin->pin > 15U ||
            pin->registers !=
                (GPIO_TypeDef*)(uintptr_t)(GPIOA_BASE +
                                           (uintptr_t)pin->port_index *
                                               UINT32_C(0x400)) ||
            ((adopting_boot && boot_pin(pin))
                 ? pin->snapshot != NULL
                 : (!valid_span(pin->snapshot, sizeof(*pin->snapshot),
                                _Alignof(uint16_t)) ||
                    !control_storage(pin->snapshot, sizeof(*pin->snapshot)))))
            return NX_ERROR_INVALID;
        uint16_t bit = (uint16_t)(1U << pin->pin);
        if ((remaining[pin->port_index - 3U] & bit) == 0U)
            return NX_ERROR_INVALID;
        remaining[pin->port_index - 3U] &= (uint16_t)~bit;
        for (size_t j = 0U; j < i; ++j) {
            if (pin->snapshot != NULL &&
                config->pins[j].snapshot == pin->snapshot)
                return NX_ERROR_INVALID;
        }
        if ((NX_STM32_FSMC_READ(RCC->AHB1ENR) &
             (UINT32_C(1) << pin->port_index)) == 0U)
            return NX_ERROR_STATE;
        if ((NX_STM32_FSMC_READ(pin->registers->LCKR) &
             ((uint32_t)bit | UINT32_C(0x10000))) ==
            ((uint32_t)bit | UINT32_C(0x10000)))
            return NX_ERROR_BUSY;
    }
    return (remaining[0] | remaining[1] | remaining[2] | remaining[3]) == 0U
               ? NX_SUCCESS
               : NX_ERROR_INVALID;
}

/** \brief The FSMC register gate remains observable while banks are dirty. */
static bool clock_present(void) {
    return (NX_STM32_FSMC_READ(RCC->AHB3ENR) & RCC_AHB3ENR_FSMCEN) != 0U;
}

/** \brief Do not implicitly share a live bank with an external transport. */
static bool foreign_bank(const nx_stm32_fsmc_t* bus) {
    for (uint32_t index = 0U; index < 8U; index += 2U) {
        bool selected = false;
        for (size_t i = 0U; i < bus->config->bank_count; ++i)
            selected |= bank_index(bus->config->banks[i].kind) == index;
        uint32_t control = NX_STM32_FSMC_READ(FSMC_Bank1->BTCR[index]);
        if (!selected && (control & FSMC_BCR1_MBKEN) != 0U) {
            /* Cold reset enables bank1's reset decoder. This exception is
             * allowed only when this owner acquired the originally off gate;
             * the caller's no-foreign-master cold contract still applies. */
            if (index != 0U || (!bus->added_gate && !bus->boot_owned) ||
                control != NX_FSMC_RESET_BANK1)
                return true;
        }
    }
    return false;
}

/** \brief Platform owns original snapshots; install only exact AF12 fields. */
static nx_result_t configure_pins(const nx_stm32_fsmc_config_t* config,
                                  bool preserve_boot) {
    uint32_t inactive_cs = 0U;
    for (size_t i = 0U; i < config->bank_count; ++i) {
        if (config->banks[i].kind == NX_STM32_FSMC_LCD_NE4)
            inactive_cs |= UINT32_C(1) << 12U;
        else if (!preserve_boot)
            inactive_cs |= UINT32_C(1) << 10U;
    }
    if (inactive_cs != 0U)
        GPIOG->BSRR = inactive_cs;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        if (preserve_boot && boot_pin(pin))
            continue;
        uint32_t selected = UINT32_C(1) << pin->pin;
        LL_GPIO_SetPinOutputType(pin->registers, selected,
                                 LL_GPIO_OUTPUT_PUSHPULL);
        LL_GPIO_SetPinPull(pin->registers, selected, LL_GPIO_PULL_NO);
        LL_GPIO_SetPinSpeed(pin->registers, selected,
                            LL_GPIO_SPEED_FREQ_VERY_HIGH);
        if (pin->pin < 8U)
            LL_GPIO_SetAFPin_0_7(pin->registers, selected, LL_GPIO_AF_12);
        else
            LL_GPIO_SetAFPin_8_15(pin->registers, selected, LL_GPIO_AF_12);
        LL_GPIO_SetPinMode(pin->registers, selected, LL_GPIO_MODE_ALTERNATE);
    }
    nx_arch_dsb();
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        uint32_t shift = (uint32_t)pin->pin * 2U;
        uint32_t af_shift = ((uint32_t)pin->pin % 8U) * 4U;
        if (((NX_STM32_FSMC_READ(pin->registers->MODER) >> shift) & 3U) !=
                LL_GPIO_MODE_ALTERNATE ||
            ((NX_STM32_FSMC_READ(pin->registers->OTYPER) >> pin->pin) & 1U) !=
                0U ||
            ((NX_STM32_FSMC_READ(pin->registers->PUPDR) >> shift) & 3U) != 0U ||
            ((NX_STM32_FSMC_READ(pin->registers->OSPEEDR) >> shift) & 3U) !=
                LL_GPIO_SPEED_FREQ_VERY_HIGH ||
            ((NX_STM32_FSMC_READ(pin->registers->AFR[pin->pin / 8U]) >>
              af_shift) &
             15U) != LL_GPIO_AF_12)
            return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

/** \brief Convert declared fields to the unchanged official LL record. */
static FSMC_NORSRAM_TimingTypeDef timing(const nx_stm32_fsmc_timing_t* value) {
    FSMC_NORSRAM_TimingTypeDef result = {value->address_setup,
                                         value->address_hold,
                                         value->data_setup,
                                         value->bus_turn,
                                         2U,
                                         2U,
                                         FSMC_ACCESS_MODE_A};
    return result;
}

/** \brief Independent expected field projection checks actual LL readback. */
static uint32_t timing_bits(const nx_stm32_fsmc_timing_t* value,
                            bool extended) {
    uint32_t result = ((uint32_t)value->address_setup << FSMC_BTR1_ADDSET_Pos) |
                      ((uint32_t)value->address_hold << FSMC_BTR1_ADDHLD_Pos) |
                      ((uint32_t)value->data_setup << FSMC_BTR1_DATAST_Pos) |
                      ((uint32_t)value->bus_turn << FSMC_BTR1_BUSTURN_Pos);
    return extended ? result : result | (UINT32_C(1) << FSMC_BTR1_CLKDIV_Pos);
}

/** \brief One bank's original values are retained before its first write. */
static nx_result_t configure_bank(nx_stm32_fsmc_t* bus, size_t i) {
    const nx_stm32_fsmc_bank_config_t* config = &bus->config->banks[i];
    nx_stm32_fsmc_bank_state_t* state = &bus->banks[i];
    uint32_t index = bank_index(config->kind);
    state->saved_control = NX_STM32_FSMC_READ(FSMC_Bank1->BTCR[index]);
    state->saved_read = NX_STM32_FSMC_READ(FSMC_Bank1->BTCR[index + 1U]);
    state->saved_write = NX_STM32_FSMC_READ(FSMC_Bank1E->BWTR[index]);
    state->captured = true;
    FSMC_NORSRAM_InitTypeDef init = {0};
    init.NSBank = index;
    init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
    init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
    init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16;
    init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
    init.ExtendedMode = FSMC_EXTENDED_MODE_ENABLE;
    FSMC_NORSRAM_TimingTypeDef read = timing(&config->read);
    FSMC_NORSRAM_TimingTypeDef write = timing(&config->write);
    if (FSMC_NORSRAM_Init(FSMC_Bank1, &init) != HAL_OK ||
        FSMC_NORSRAM_Timing_Init(FSMC_Bank1, &read, index) != HAL_OK ||
        FSMC_NORSRAM_Extended_Timing_Init(FSMC_Bank1E, &write, index,
                                          FSMC_EXTENDED_MODE_ENABLE) != HAL_OK)
        return NX_ERROR_IO;
    __FSMC_NORSRAM_ENABLE(FSMC_Bank1, index);
    nx_arch_dsb();
    const uint32_t control_mask =
        (uint32_t)(FSMC_BCR1_MBKEN | FSMC_BCR1_MUXEN | FSMC_BCR1_MTYP |
                   FSMC_BCR1_MWID | FSMC_BCR1_FACCEN | FSMC_BCR1_BURSTEN |
                   FSMC_BCR1_WREN | FSMC_BCR1_WAITEN | FSMC_BCR1_EXTMOD |
                   FSMC_BCR1_ASYNCWAIT | FSMC_BCR1_CBURSTRW);
    const uint32_t expected =
        (uint32_t)(FSMC_BCR1_MBKEN | FSMC_NORSRAM_MEM_BUS_WIDTH_16 |
                   FSMC_WRITE_OPERATION_ENABLE | FSMC_EXTENDED_MODE_ENABLE);
    if ((NX_STM32_FSMC_READ(FSMC_Bank1->BTCR[index]) & control_mask) !=
            expected ||
        NX_STM32_FSMC_READ(FSMC_Bank1->BTCR[index + 1U]) !=
            timing_bits(&config->read, false) ||
        (NX_STM32_FSMC_READ(FSMC_Bank1E->BWTR[index]) &
         (uint32_t)(FSMC_BWTR1_ADDSET | FSMC_BWTR1_ADDHLD | FSMC_BWTR1_DATAST |
                    FSMC_BWTR1_BUSTURN | FSMC_BWTR1_ACCMOD)) !=
            timing_bits(&config->write, true))
        return NX_ERROR_IO;
    return NX_SUCCESS;
}

static nx_result_t admit(nx_stm32_fsmc_t* bus,
                         const nx_stm32_fsmc_config_t* config,
                         nx_stm32_fsmc_bank_state_t* banks, size_t bank_count,
                         const nx_stm32_sram_boot_plan_t* boot_plan) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_span(bus, sizeof(*bus), _Alignof(nx_stm32_fsmc_t)) ||
        !control_storage(bus, sizeof(*bus)) ||
        !valid_span(config, sizeof(*config),
                    _Alignof(nx_stm32_fsmc_config_t)) ||
        !fact_storage(config, sizeof(*config)))
        return NX_ERROR_INVALID;
    if (config->boot_backed != (boot_plan != NULL))
        return NX_ERROR_UNSUPPORTED;
    if (config->bank_count == 0U || config->bank_count > 2U ||
        bank_count != config->bank_count || config->pin_count == 0U ||
        config->pin_count > 41U || config->clock.read == NULL ||
        !valid_span(config->banks, bank_count * sizeof(*config->banks),
                    _Alignof(nx_stm32_fsmc_bank_config_t)) ||
        !fact_storage(config->banks, bank_count * sizeof(*config->banks)) ||
        !valid_span(banks, bank_count * sizeof(*banks),
                    _Alignof(nx_stm32_fsmc_bank_state_t)) ||
        !control_storage(banks, bank_count * sizeof(*banks)) ||
        !valid_span(config->pins, config->pin_count * sizeof(*config->pins),
                    _Alignof(nx_stm32_platform_pin_config_t)) ||
        !fact_storage(config->pins,
                      config->pin_count * sizeof(*config->pins)) ||
        overlap(bus, sizeof(*bus), config, sizeof(*config)) ||
        overlap(bus, sizeof(*bus), banks, bank_count * sizeof(*banks)) ||
        overlap(bus, sizeof(*bus), config->banks,
                bank_count * sizeof(*config->banks)) ||
        overlap(bus, sizeof(*bus), config->pins,
                config->pin_count * sizeof(*config->pins)) ||
        overlap(config, sizeof(*config), config->banks,
                bank_count * sizeof(*config->banks)) ||
        overlap(config, sizeof(*config), config->pins,
                config->pin_count * sizeof(*config->pins)) ||
        overlap(config->banks, bank_count * sizeof(*config->banks),
                config->pins, config->pin_count * sizeof(*config->pins)) ||
        overlap(config, sizeof(*config), banks, bank_count * sizeof(*banks)) ||
        overlap(config->banks, bank_count * sizeof(*config->banks), banks,
                bank_count * sizeof(*banks)) ||
        overlap(config->pins, config->pin_count * sizeof(*config->pins), banks,
                bank_count * sizeof(*banks)))
        return NX_ERROR_INVALID;
    if (contains_anchor(bus, sizeof(*bus), config->clock.context) ||
        contains_anchor(banks, bank_count * sizeof(*banks),
                        config->clock.context))
        return NX_ERROR_INVALID;
    if (bus->config != NULL || bus->banks != NULL || bus->prepared ||
        bus->added_gate || (boot_plan == NULL && bus->boot_owned))
        return NX_ERROR_BUSY;
    if (boot_plan != NULL && (!bus->boot_owned || bus->epoch != 0U))
        return NX_ERROR_INVALID;
    if (bus->epoch == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    for (size_t i = 0U; i < bank_count; ++i) {
        if ((config->banks[i].kind != NX_STM32_FSMC_SRAM_NE3 &&
             config->banks[i].kind != NX_STM32_FSMC_LCD_NE4) ||
            !timing_valid(&config->banks[i].read) ||
            !timing_valid(&config->banks[i].write) ||
            (i != 0U && config->banks[0].kind == config->banks[i].kind))
            return NX_ERROR_INVALID;
        if (banks[i].captured || banks[i].lease != NULL)
            return NX_ERROR_BUSY;
    }
    if (boot_plan != NULL) {
        if (!valid_span(boot_plan, sizeof(*boot_plan),
                        _Alignof(nx_stm32_sram_boot_plan_t)))
            return NX_ERROR_INVALID;
        bool exact_sram = false;
        for (size_t i = 0U; i < bank_count; ++i)
            exact_sram |= config->banks[i].kind == NX_STM32_FSMC_SRAM_NE3 &&
                          &config->banks[i] == boot_plan->bank;
        if (!exact_sram ||
            overlap(bus, sizeof(*bus), boot_plan, sizeof(*boot_plan)) ||
            overlap(banks, bank_count * sizeof(*banks), boot_plan,
                    sizeof(*boot_plan)))
            return NX_ERROR_INVALID;
    }
    nx_result_t result = pins_valid(config, boot_plan != NULL);
    if (result != NX_SUCCESS)
        return result;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        if (config->pins[i].snapshot != NULL &&
            (!outside_configuration(bus, config, banks,
                                    config->pins[i].snapshot,
                                    sizeof(*config->pins[i].snapshot)) ||
             (boot_plan != NULL && overlap(config->pins[i].snapshot,
                                           sizeof(*config->pins[i].snapshot),
                                           boot_plan, sizeof(*boot_plan)))))
            return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}

nx_result_t nx_stm32_fsmc_prepare(nx_stm32_fsmc_t* bus,
                                  const nx_stm32_fsmc_config_t* config,
                                  nx_stm32_fsmc_bank_state_t* banks,
                                  size_t bank_count) {
    nx_result_t result = admit(bus, config, banks, bank_count, NULL);
    if (result != NX_SUCCESS)
        return result;
    bus->config = config;
    bus->banks = banks;
    bus->added_gate = !clock_present();
    RCC->AHB3ENR |= (uint32_t)RCC_AHB3ENR_FSMCEN;
    nx_arch_dsb();
    if (!clock_present())
        result = NX_ERROR_IO;
    if (result == NX_SUCCESS && foreign_bank(bus))
        result = NX_ERROR_BUSY;
    for (size_t i = 0U; result == NX_SUCCESS && i < bank_count; ++i) {
        if ((NX_STM32_FSMC_READ(
                 FSMC_Bank1->BTCR[bank_index(config->banks[i].kind)]) &
             FSMC_BCR1_MBKEN) != 0U)
            result = NX_ERROR_BUSY;
    }
    if (result == NX_SUCCESS)
        result = configure_pins(config, false);
    for (size_t i = 0U; result == NX_SUCCESS && i < bank_count; ++i)
        result = configure_bank(bus, i);
    if (result == NX_SUCCESS)
        bus->prepared = true;
    else
        (void)nx_stm32_fsmc_stop(bus);
    return result;
}

nx_result_t nx_stm32_fsmc_adopt_proven(nx_stm32_fsmc_t* bus,
                                       const nx_stm32_fsmc_config_t* config,
                                       nx_stm32_fsmc_bank_state_t* banks,
                                       size_t bank_count,
                                       const nx_stm32_sram_boot_plan_t* plan) {
    if (plan == NULL)
        return NX_ERROR_INVALID;
    nx_result_t result = admit(bus, config, banks, bank_count, plan);
    if (result != NX_SUCCESS)
        return result;
    nx_stm32_fsmc_t observed_owner = {0};
    observed_owner.config = config;
    observed_owner.boot_owned = true;
    if (!clock_present())
        return NX_ERROR_IO;
    if (foreign_bank(&observed_owner))
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < bank_count; ++i) {
        if (config->banks[i].kind != NX_STM32_FSMC_SRAM_NE3 &&
            (NX_STM32_FSMC_READ(
                 FSMC_Bank1->BTCR[bank_index(config->banks[i].kind)]) &
             FSMC_BCR1_MBKEN) != 0U)
            return NX_ERROR_BUSY;
    }
    /* The real reset proof has succeeded. The statically selected receipt
     * already protects live globals before this call. Bind runtime facts,
     * retaining that receipt even if optional LCD setup later fails. */
    bus->config = config;
    bus->banks = banks;
    result = configure_pins(config, true);
    for (size_t i = 0U; result == NX_SUCCESS && i < bank_count; ++i) {
        if (config->banks[i].kind != NX_STM32_FSMC_SRAM_NE3)
            result = configure_bank(bus, i);
    }
    if (result == NX_SUCCESS)
        bus->prepared = true;
    return result;
}

nx_result_t nx_stm32_fsmc_stop(nx_stm32_fsmc_t* bus) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_span(bus, sizeof(*bus), _Alignof(nx_stm32_fsmc_t)) ||
        !control_storage(bus, sizeof(*bus)))
        return NX_ERROR_INVALID;
    if (bus->boot_owned)
        return NX_ERROR_BUSY;
    if (bus->config == NULL)
        return NX_SUCCESS;
    bool dirty = bus->prepared;
    for (size_t i = 0U; i < bus->config->bank_count; ++i) {
        if (bus->banks[i].lease != NULL)
            return NX_ERROR_BUSY;
        dirty |= bus->banks[i].captured;
    }
    if (dirty && !clock_present())
        return NX_ERROR_IO;
    if (dirty && foreign_bank(bus))
        return NX_ERROR_BUSY;
    /* Complete caller-joined CPU stores before any decoder is withdrawn.
     * nx_arch_dsb also carries the compiler memory clobber on Cortex-M. */
    if (dirty)
        nx_arch_dsb();
    for (size_t reverse = bus->config->bank_count; reverse > 0U; --reverse) {
        size_t i = reverse - 1U;
        nx_stm32_fsmc_bank_state_t* state = &bus->banks[i];
        if (!state->captured)
            continue;
        uint32_t index = bank_index(bus->config->banks[i].kind);
        __FSMC_NORSRAM_DISABLE(FSMC_Bank1, index);
        nx_arch_dsb();
        if ((NX_STM32_FSMC_READ(FSMC_Bank1->BTCR[index]) & FSMC_BCR1_MBKEN) !=
            0U)
            return NX_ERROR_IO;
        FSMC_Bank1->BTCR[index + 1U] = state->saved_read;
        FSMC_Bank1E->BWTR[index] = state->saved_write;
        FSMC_Bank1->BTCR[index] = state->saved_control;
        nx_arch_dsb();
        if (NX_STM32_FSMC_READ(FSMC_Bank1->BTCR[index]) !=
                state->saved_control ||
            NX_STM32_FSMC_READ(FSMC_Bank1->BTCR[index + 1U]) !=
                state->saved_read ||
            NX_STM32_FSMC_READ(FSMC_Bank1E->BWTR[index]) != state->saved_write)
            return NX_ERROR_IO;
        *state = (nx_stm32_fsmc_bank_state_t){0};
    }
    bus->prepared = false;
    if (bus->added_gate) {
        nx_arch_dsb();
        RCC->AHB3ENR &= ~(uint32_t)RCC_AHB3ENR_FSMCEN;
        nx_arch_dsb();
        if (clock_present())
            return NX_ERROR_IO;
    }
    bus->config = NULL;
    bus->banks = NULL;
    bus->added_gate = false;
    return NX_SUCCESS;
}

/** \brief Mutable endpoints must remain disjoint from bus facts/receipts. */
static bool outside_bus(const nx_stm32_fsmc_t* bus, const void* object,
                        size_t bytes) {
    if (!outside_configuration(bus, bus->config, bus->banks, object, bytes))
        return false;
    for (size_t i = 0U; i < bus->config->pin_count; ++i) {
        if (bus->config->pins[i].snapshot != NULL &&
            overlap(object, bytes, bus->config->pins[i].snapshot,
                    sizeof(*bus->config->pins[i].snapshot)))
            return false;
    }
    for (size_t i = 0U; i < bus->config->bank_count; ++i) {
        if (bus->banks[i].lease == NULL)
            continue;
        size_t lease_bytes = bus->config->banks[i].kind == NX_STM32_FSMC_LCD_NE4
                                 ? sizeof(nx_stm32_fsmc_lcd_t)
                                 : sizeof(nx_stm32_fsmc_lease_t);
        if (overlap(object, bytes, bus->banks[i].lease, lease_bytes))
            return false;
    }
    return true;
}

/** \brief At most one caller-owned lease owns each physical selected bank. */
static nx_result_t acquire(nx_stm32_fsmc_t* bus, nx_stm32_fsmc_lease_t* lease,
                           nx_stm32_fsmc_kind_t kind) {
    if (!valid_span(bus, sizeof(*bus), _Alignof(nx_stm32_fsmc_t)) ||
        !control_storage(bus, sizeof(*bus)) ||
        !valid_span(lease, sizeof(*lease), _Alignof(nx_stm32_fsmc_lease_t)) ||
        !control_storage(lease, sizeof(*lease)))
        return NX_ERROR_INVALID;
    if (!bus->prepared || bus->config == NULL)
        return NX_ERROR_STATE;
    /* A whole-bank runtime loan cannot alias DATA/BSS/NOINIT already backing
     * live globals. A disjoint free-region allocator is a separate contract. */
    if (kind == NX_STM32_FSMC_SRAM_NE3 && bus->boot_owned)
        return NX_ERROR_UNSUPPORTED;
    if (!outside_bus(bus, lease, sizeof(*lease)))
        return NX_ERROR_INVALID;
    if (lease->owner != NULL)
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < bus->config->bank_count; ++i) {
        if (bus->config->banks[i].kind != kind)
            continue;
        if (bus->banks[i].lease != NULL)
            return NX_ERROR_BUSY;
        if (bus->epoch == UINT64_MAX)
            return NX_ERROR_EXHAUSTED;
        if (!clock_present() ||
            (NX_STM32_FSMC_READ(FSMC_Bank1->BTCR[bank_index(kind)]) &
             FSMC_BCR1_MBKEN) == 0U)
            return NX_ERROR_IO;
        ++bus->epoch;
        *lease = (nx_stm32_fsmc_lease_t){
            bus,
            kind == NX_STM32_FSMC_SRAM_NE3 ? NX_FSMC_SRAM_ADDRESS
                                           : NX_FSMC_LCD_COMMAND,
            kind == NX_STM32_FSMC_SRAM_NE3 ? NX_FSMC_SRAM_BYTES : 0x2002U,
            bus->epoch};
        bus->banks[i].lease = lease;
        return NX_SUCCESS;
    }
    return NX_ERROR_UNSUPPORTED;
}

nx_result_t nx_stm32_fsmc_sram_acquire(nx_stm32_fsmc_t* bus,
                                       nx_stm32_fsmc_lease_t* lease) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    return acquire(bus, lease, NX_STM32_FSMC_SRAM_NE3);
}

nx_result_t nx_stm32_fsmc_release(nx_stm32_fsmc_lease_t* lease,
                                  uint64_t epoch) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_span(lease, sizeof(*lease), _Alignof(nx_stm32_fsmc_lease_t)) ||
        !control_storage(lease, sizeof(*lease)) || lease->owner == NULL)
        return NX_ERROR_INVALID;
    nx_stm32_fsmc_t* bus = lease->owner;
    if (!valid_span(bus, sizeof(*bus), _Alignof(nx_stm32_fsmc_t)) ||
        !control_storage(bus, sizeof(*bus)))
        return NX_ERROR_INVALID;
    if (!bus->prepared || bus->config == NULL)
        return NX_ERROR_STATE;
    for (size_t i = 0U; i < bus->config->bank_count; ++i) {
        if (bus->banks[i].lease != lease)
            continue;
        if (lease->epoch != epoch)
            return NX_ERROR_STATE;
        if (bus->config->banks[i].kind == NX_STM32_FSMC_LCD_NE4 &&
            ((nx_stm32_fsmc_lcd_t*)lease)->in_call)
            return NX_ERROR_BUSY;
        /* The release publication follows final CPU access. DMA completion
         * remains an explicit caller join; a CPU barrier cannot prove it. */
        nx_arch_dsb();
        bus->banks[i].lease = NULL;
        *lease = (nx_stm32_fsmc_lease_t){0};
        return NX_SUCCESS;
    }
    return NX_ERROR_INVALID;
}

/** \brief Check a live exact LCD loan, hardware and one monotonic deadline. */
static nx_result_t lcd_check(nx_stm32_fsmc_lcd_t* lcd, nx_time_us_t deadline) {
    nx_stm32_fsmc_t* bus = lcd->lease.owner;
    if (bus == NULL || !bus->prepared || bus->config == NULL)
        return NX_ERROR_STATE;
    bool found = false;
    for (size_t i = 0U; i < bus->config->bank_count; ++i)
        found |= bus->config->banks[i].kind == NX_STM32_FSMC_LCD_NE4 &&
                 bus->banks[i].lease == &lcd->lease;
    if (!found)
        return NX_ERROR_STATE;
    if (!clock_present() ||
        (NX_STM32_FSMC_READ(FSMC_Bank1->BTCR[FSMC_NORSRAM_BANK4]) &
         FSMC_BCR1_MBKEN) == 0U)
        return NX_ERROR_IO;
    nx_time_us_t now = bus->config->clock.read(bus->config->clock.context);
    if (now == NX_DEADLINE_NEVER || now < lcd->last_clock)
        return NX_ERROR_STATE;
    lcd->last_clock = now;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    return now >= deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}

/** \brief A command is one completed halfword, never a queued transfer. */
static nx_result_t lcd_command(void* context, uint16_t command,
                               nx_time_us_t deadline) {
    nx_stm32_fsmc_lcd_t* lcd = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_span(lcd, sizeof(*lcd), _Alignof(nx_stm32_fsmc_lcd_t)) ||
        !control_storage(lcd, sizeof(*lcd)))
        return NX_ERROR_INVALID;
    if (lcd->in_call)
        return NX_ERROR_BUSY;
    lcd->in_call = true;
    nx_result_t result = lcd_check(lcd, deadline);
    if (result == NX_SUCCESS) {
        NX_STM32_FSMC_WRITE16(
            (volatile uint16_t*)(uintptr_t)NX_FSMC_LCD_COMMAND, command);
        nx_arch_dsb();
        result = lcd_check(lcd, deadline);
    }
    lcd->in_call = false;
    return result;
}

/** \brief External SRAM payloads require the same observed live NE3 owner. */
static bool payload_storage(const nx_stm32_fsmc_t* bus, const void* data,
                            size_t bytes, bool writable) {
    if (physical_overlap(data, bytes, UINT64_C(0x40000000),
                         UINT64_C(0x60000000)) ||
        physical_overlap(data, bytes, UINT64_C(0xe0000000),
                         UINT64_C(0x100000000)) ||
        (writable && physical_overlap(data, bytes, UINT64_C(0x08000000),
                                      UINT64_C(0x10000000))))
        return false;
    if (!physical_overlap(data, bytes, UINT64_C(0x60000000),
                          UINT64_C(0xe0000000)))
        return true;
    uintptr_t address = (uintptr_t)data;
    if (bus == NULL || !bus->prepared || bus->config == NULL ||
        address < NX_FSMC_SRAM_ADDRESS ||
        address >= NX_FSMC_SRAM_ADDRESS + NX_FSMC_SRAM_BYTES ||
        bytes > NX_FSMC_SRAM_ADDRESS + NX_FSMC_SRAM_BYTES - address)
        return false;
    bool owns = bus->boot_owned;
    for (size_t i = 0U; i < bus->config->bank_count; ++i)
        owns |= bus->config->banks[i].kind == NX_STM32_FSMC_SRAM_NE3 &&
                bus->banks[i].lease != NULL;
    return owns && clock_present() &&
           (NX_STM32_FSMC_READ(FSMC_Bank1->BTCR[FSMC_NORSRAM_BANK3]) &
            FSMC_BCR1_MBKEN) != 0U;
}

/** \brief The bounded word loop owns no reference after its final return. */
static nx_result_t lcd_words(nx_stm32_fsmc_lcd_t* lcd, const uint16_t* tx,
                             uint16_t* rx, size_t count, size_t* done,
                             nx_time_us_t deadline) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    void* data = rx != NULL ? (void*)rx : (void*)(uintptr_t)tx;
    if (!valid_span(lcd, sizeof(*lcd), _Alignof(nx_stm32_fsmc_lcd_t)) ||
        !control_storage(lcd, sizeof(*lcd)) || count == 0U || count > 76800U ||
        !valid_span(data, count * sizeof(uint16_t), _Alignof(uint16_t)) ||
        !valid_span(done, sizeof(*done), _Alignof(size_t)) ||
        !control_storage(done, sizeof(*done)) ||
        overlap(lcd, sizeof(*lcd), data, count * sizeof(uint16_t)) ||
        overlap(lcd, sizeof(*lcd), done, sizeof(*done)) ||
        overlap(data, count * sizeof(uint16_t), done, sizeof(*done)))
        return NX_ERROR_INVALID;
    if (lcd->in_call)
        return NX_ERROR_BUSY;
    nx_stm32_fsmc_t* bus = lcd->lease.owner;
    if (!payload_storage(bus, data, count * sizeof(uint16_t), rx != NULL))
        return NX_ERROR_INVALID;
    if (bus != NULL && bus->config != NULL &&
        (!outside_bus(bus, data, count * sizeof(uint16_t)) ||
         !outside_bus(bus, done, sizeof(*done))))
        return NX_ERROR_INVALID;
    lcd->in_call = true;
    *done = 0U;
    nx_result_t result = lcd_check(lcd, deadline);
    while (result == NX_SUCCESS && *done < count) {
        size_t end = count - *done > 32U ? *done + 32U : count;
        while (*done < end) {
            if (rx != NULL)
                rx[*done] = NX_STM32_FSMC_READ16(
                    (const volatile uint16_t*)(uintptr_t)NX_FSMC_LCD_DATA);
            else
                NX_STM32_FSMC_WRITE16(
                    (volatile uint16_t*)(uintptr_t)NX_FSMC_LCD_DATA, tx[*done]);
            ++*done;
        }
        nx_arch_dsb();
        result = lcd_check(lcd, deadline);
    }
    lcd->in_call = false;
    return result;
}

/** \brief Immutable settled port wrapper, no additional transport owner. */
static nx_result_t lcd_write(void* context, const uint16_t* words, size_t count,
                             size_t* transferred, nx_time_us_t deadline) {
    return lcd_words(context, words, NULL, count, transferred, deadline);
}

/** \brief Read words only from the same selected data-register window. */
static nx_result_t lcd_read(void* context, uint16_t* words, size_t count,
                            size_t* transferred, nx_time_us_t deadline) {
    return lcd_words(context, NULL, words, count, transferred, deadline);
}

nx_result_t nx_stm32_fsmc_lcd_prepare(nx_stm32_fsmc_t* bus,
                                      nx_stm32_fsmc_lcd_t* lcd) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_span(bus, sizeof(*bus), _Alignof(nx_stm32_fsmc_t)) ||
        !control_storage(bus, sizeof(*bus)) ||
        !valid_span(lcd, sizeof(*lcd), _Alignof(nx_stm32_fsmc_lcd_t)) ||
        !control_storage(lcd, sizeof(*lcd)))
        return NX_ERROR_INVALID;
    if (!bus->prepared || bus->config == NULL)
        return NX_ERROR_STATE;
    if (!outside_bus(bus, lcd, sizeof(*lcd)))
        return NX_ERROR_INVALID;
    if (lcd->in_call)
        return NX_ERROR_BUSY;
    lcd->in_call = true;
    nx_time_us_t now = bus->config->clock.read(bus->config->clock.context);
    nx_result_t result = now == NX_DEADLINE_NEVER
                             ? NX_ERROR_STATE
                             : acquire(bus, &lcd->lease, NX_STM32_FSMC_LCD_NE4);
    if (result == NX_SUCCESS)
        lcd->last_clock = now;
    lcd->in_call = false;
    return result;
}

nx_result_t nx_stm32_fsmc_lcd_close(nx_stm32_fsmc_lcd_t* lcd) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_span(lcd, sizeof(*lcd), _Alignof(nx_stm32_fsmc_lcd_t)) ||
        !control_storage(lcd, sizeof(*lcd)))
        return NX_ERROR_INVALID;
    if (lcd->in_call)
        return NX_ERROR_BUSY;
    if (lcd->lease.owner == NULL)
        return NX_SUCCESS;
    return nx_stm32_fsmc_release(&lcd->lease, lcd->lease.epoch);
}

/** \brief A software phase query does not read or power hardware. */
static nx_external_bus_state_t bus_state(const void* context) {
    const nx_stm32_fsmc_t* bus = context;
    if (!valid_span(bus, sizeof(*bus), _Alignof(nx_stm32_fsmc_t)) ||
        !control_storage(bus, sizeof(*bus)))
        return NX_EXTERNAL_BUS_UNAVAILABLE;
    if (bus->prepared)
        return NX_EXTERNAL_BUS_PREPARED;
    return bus->config == NULL && !bus->boot_owned
               ? NX_EXTERNAL_BUS_STOPPED
               : NX_EXTERNAL_BUS_CLEANUP_PENDING;
}

/** \brief Public stop reaches the one actual hardware owner directly. */
static nx_result_t bus_stop(void* context) {
    return nx_stm32_fsmc_stop(context);
}

/** \brief Public acquisition exposes a direct CPU window, not per-load I/O. */
static nx_result_t window_acquire(void* context,
                                  nx_memory_window_lease_t* lease) {
    return nx_stm32_fsmc_sram_acquire(context, lease);
}

/** \brief A foreign public face cannot release another bus's exact loan. */
static nx_result_t
window_release(void* context, nx_memory_window_lease_t* lease, uint64_t epoch) {
    if (context == NULL ||
        !valid_span(lease, sizeof(*lease), _Alignof(nx_stm32_fsmc_lease_t)) ||
        !control_storage(lease, sizeof(*lease)) || lease->owner != context)
        return NX_ERROR_INVALID;
    return nx_stm32_fsmc_release(lease, epoch);
}

/** \brief Geometry is a fact query, never a whole-bank access grant. */
static nx_result_t window_geometry(const void* context,
                                   nx_memory_window_geometry_t* geometry) {
    const nx_stm32_fsmc_t* bus = context;
    if (!valid_span(bus, sizeof(*bus), _Alignof(nx_stm32_fsmc_t)) ||
        !control_storage(bus, sizeof(*bus)) ||
        !valid_span(geometry, sizeof(*geometry),
                    _Alignof(nx_memory_window_geometry_t)) ||
        !control_storage(geometry, sizeof(*geometry)))
        return NX_ERROR_INVALID;
    if (bus->config == NULL)
        return NX_ERROR_STATE;
    if (!outside_bus(bus, geometry, sizeof(*geometry)))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < bus->config->bank_count; ++i) {
        if (bus->config->banks[i].kind == NX_STM32_FSMC_SRAM_NE3) {
            *geometry = (nx_memory_window_geometry_t){
                NX_FSMC_SRAM_ADDRESS, NX_FSMC_SRAM_BYTES, bus->boot_owned};
            return NX_SUCCESS;
        }
    }
    return NX_ERROR_UNSUPPORTED;
}

const nx_external_bus_ops_t nx_stm32_fsmc_external_bus_ops = {bus_state,
                                                              bus_stop};
const nx_parallel16_ops_t nx_stm32_fsmc_parallel16_ops = {lcd_command,
                                                          lcd_write, lcd_read};
const nx_memory_window_ops_t nx_stm32_fsmc_memory_window_ops = {
    window_acquire, window_release, window_geometry};
