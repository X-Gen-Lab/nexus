/**
 * \file            fsmc.c
 * \brief           Exact shared FSMC bank ownership and settled LCD transfers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
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
    return (config->parameters == NULL ||
            !overlap(config->parameters,
                     config->bank_count * sizeof(*config->parameters), object,
                     bytes)) &&
           !overlap(bus, sizeof(*bus), object, bytes) &&
           !overlap(config, sizeof(*config), object, bytes) &&
           !overlap(banks, config->bank_count * sizeof(*banks), object,
                    bytes) &&
           !overlap(config->banks, config->bank_count * sizeof(*config->banks),
                    object, bytes) &&
           !overlap(config->pins, config->pin_count * sizeof(*config->pins),
                    object, bytes) &&
           !contains_anchor(object, bytes, config->clock.context);
}

/** \brief Legacy profiles stay byte-for-byte independent of boot descriptors.
 */
static nx_stm32_fsmc_parameters_t
parameters(const nx_stm32_fsmc_config_t* config, size_t i) {
    nx_stm32_fsmc_kind_t kind = config->banks[i].kind;
    if (kind != NX_STM32_FSMC_SRAM_NE3 && kind != NX_STM32_FSMC_LCD_NE4) {
        /* Admission rejects absent extended facts; never synthesize a bank. */
        if (config->parameters == NULL)
            return (nx_stm32_fsmc_parameters_t){0};
        return config->parameters[i];
    }
    nx_stm32_fsmc_parameters_t result = {0};
    result.bank = kind == NX_STM32_FSMC_SRAM_NE3 ? 3U : 4U;
    result.width = 16U;
    result.bytes =
        kind == NX_STM32_FSMC_SRAM_NE3 ? NX_FSMC_SRAM_BYTES : 0x2002U;
    result.address_lines = 19U;
    result.byte_lanes = kind == NX_STM32_FSMC_SRAM_NE3;
    result.extended_mode = true;
    result.clock_division = 2U;
    result.data_latency = 2U;
    return result;
}
/** \brief NAND and NORSRAM have independent hardware decoder indices. */
static bool nand_bank(const nx_stm32_fsmc_config_t* config, size_t i) {
    return config->banks[i].kind == NX_STM32_FSMC_NAND;
}
static uint32_t bank_index(const nx_stm32_fsmc_config_t* config, size_t i) {
    return ((uint32_t)parameters(config, i).bank - 1U) * 2U;
}
static volatile uint32_t* control_register(const nx_stm32_fsmc_config_t* config,
                                           size_t i) {
    if (nand_bank(config, i))
        return parameters(config, i).bank == 2U ? &FSMC_Bank2_3->PCR2
                                                : &FSMC_Bank2_3->PCR3;
    return &FSMC_Bank1->BTCR[bank_index(config, i)];
}
static volatile uint32_t* read_register(const nx_stm32_fsmc_config_t* config,
                                        size_t i) {
    if (nand_bank(config, i))
        return parameters(config, i).bank == 2U ? &FSMC_Bank2_3->PMEM2
                                                : &FSMC_Bank2_3->PMEM3;
    return &FSMC_Bank1->BTCR[bank_index(config, i) + 1U];
}
static volatile uint32_t* write_register(const nx_stm32_fsmc_config_t* config,
                                         size_t i) {
    if (nand_bank(config, i))
        return parameters(config, i).bank == 2U ? &FSMC_Bank2_3->PATT2
                                                : &FSMC_Bank2_3->PATT3;
    return &FSMC_Bank1E->BWTR[bank_index(config, i)];
}
static uint32_t bank_enable(const nx_stm32_fsmc_config_t* config, size_t i) {
    return nand_bank(config, i) ? (uint32_t)FSMC_PCR2_PBKEN
                                : (uint32_t)FSMC_BCR1_MBKEN;
}
static uintptr_t bank_address(const nx_stm32_fsmc_config_t* config, size_t i) {
    uint32_t bank = parameters(config, i).bank;
    return nand_bank(config, i)
               ? UINT32_C(0x70000000) + (bank - 2U) * UINT32_C(0x10000000)
               : UINT32_C(0x60000000) + (bank - 1U) * UINT32_C(0x04000000);
}
static size_t bank_bytes(const nx_stm32_fsmc_config_t* config, size_t i) {
    return nand_bank(config, i) ? UINT32_C(0x40000)
                                : parameters(config, i).bytes;
}
/** \brief Reject unsupported electrical fields before any pad/gate change. */
static bool profile_valid(const nx_stm32_fsmc_config_t* config, size_t i) {
    nx_stm32_fsmc_kind_t kind = config->banks[i].kind;
    if (kind == NX_STM32_FSMC_SRAM_NE3 || kind == NX_STM32_FSMC_LCD_NE4)
        return true;
    if (config->parameters == NULL ||
        (kind != NX_STM32_FSMC_PSRAM && kind != NX_STM32_FSMC_NOR &&
         kind != NX_STM32_FSMC_NAND) ||
        (config->package_pins != 100U && config->package_pins != 144U))
        return false;
    nx_stm32_fsmc_parameters_t p = parameters(config, i);
    if (p.width != 8U && p.width != 16U)
        return false;
    if (kind == NX_STM32_FSMC_NAND) {
#if !defined(NEXUS_STM32_FSMC_NAND)
        return false;
#else
        return (p.bank == 2U || p.bank == 3U) && p.bytes == 0U &&
               p.address_lines == 0U && !p.multiplexed && !p.byte_lanes &&
               !p.extended_mode && p.burst_page_bytes == 0U && !p.burst_read &&
               !p.burst_write && !p.asynchronous_wait && !p.wait_high &&
               !p.wait_before && !p.wrap && p.clock_division == 0U &&
               p.data_latency == 0U && p.access_mode == 0U && p.tclr <= 15U &&
               p.tar <= 15U && p.ecc_bytes >= 256U && p.ecc_bytes <= 8192U &&
               (p.ecc_bytes & (p.ecc_bytes - 1U)) == 0U &&
               p.common.setup < 255U && p.common.wait < 255U &&
               p.common.hold < 255U && p.common.hiz < 255U &&
               p.attribute.setup < 255U && p.attribute.wait < 255U &&
               p.attribute.hold < 255U && p.attribute.hiz < 255U;
#endif
    }
    if (p.address_lines < 1U || p.address_lines > 26U)
        return false;
    uint64_t capacity = (UINT64_C(1) << p.address_lines) * (p.width / 8U);
    return p.bank >= 1U && p.bank <= 4U && p.address_lines >= 1U &&
           p.address_lines <= 26U && p.bytes != 0U &&
           p.bytes <= UINT32_C(0x4000000) && p.bytes <= capacity &&
           p.bytes % (p.width / 8U) == 0U && p.clock_division >= 2U &&
           p.clock_division <= 16U && p.data_latency >= 2U &&
           p.data_latency <= 17U && p.access_mode <= 3U &&
           (!p.multiplexed || !p.extended_mode) &&
           (p.extended_mode || (p.access_mode == 0U &&
                                config->banks[i].read.address_setup ==
                                    config->banks[i].write.address_setup &&
                                config->banks[i].read.address_hold ==
                                    config->banks[i].write.address_hold &&
                                config->banks[i].read.data_setup ==
                                    config->banks[i].write.data_setup &&
                                config->banks[i].read.bus_turn ==
                                    config->banks[i].write.bus_turn)) &&
           (p.burst_page_bytes == 0U ||
            (kind == NX_STM32_FSMC_PSRAM && p.burst_read &&
             (p.burst_page_bytes == 128U || p.burst_page_bytes == 256U ||
              p.burst_page_bytes == 512U || p.burst_page_bytes == 1024U))) &&
           (!p.burst_write || (p.burst_read && kind == NX_STM32_FSMC_PSRAM)) &&
           (!p.wrap || p.burst_read) &&
           (!p.burst_read || NEXUS_CORE_HZ / p.clock_division <= 60000000U) &&
           (!p.wait_high || p.wait_signal || p.asynchronous_wait) &&
           (!p.wait_before || p.wait_signal) && p.ecc_bytes == 0U &&
           p.tclr == 0U && p.tar == 0U;
}

/** \brief Reject illegal encoded fields before the official LL sees them. */
static bool timing_valid(const nx_stm32_fsmc_timing_t* timing) {
    return timing->address_setup <= 15U && timing->address_hold >= 1U &&
           timing->address_hold <= 15U && timing->data_setup >= 1U &&
           timing->bus_turn <= 15U;
}

/** \brief Exact DS8626 AF12 address/data pad map for F407VE/ZG only. */
static void required_pins(const nx_stm32_fsmc_config_t* config,
                          uint16_t masks[7]) {
    static const uint8_t data[16][2] = {
        {3, 14}, {3, 15}, {3, 0},  {3, 1},  {4, 7},  {4, 8}, {4, 9}, {4, 10},
        {4, 11}, {4, 12}, {4, 13}, {4, 14}, {4, 15}, {3, 8}, {3, 9}, {3, 10}};
    static const uint8_t address[26][2] = {
        {5, 0},  {5, 1},  {5, 2},  {5, 3},  {5, 4},  {5, 5}, {5, 12},
        {5, 13}, {5, 14}, {5, 15}, {6, 0},  {6, 1},  {6, 2}, {6, 3},
        {6, 4},  {6, 5},  {3, 11}, {3, 12}, {3, 13}, {4, 3}, {4, 4},
        {4, 5},  {4, 6},  {4, 2},  {6, 13}, {6, 14}};
    for (size_t port = 0U; port < 7U; ++port)
        masks[port] = 0U;
    for (size_t i = 0U; i < config->bank_count; ++i) {
        nx_stm32_fsmc_parameters_t p = parameters(config, i);
        for (size_t bit = 0U; bit < p.width; ++bit)
            masks[data[bit][0]] |= (uint16_t)(1U << data[bit][1]);
        masks[3] |= UINT16_C(0x0030); /* NOE/NWE */
        if (nand_bank(config, i)) {
            masks[3] |= UINT16_C(0x1800); /* A16=CLE, A17=ALE */
            if (p.bank == 2U)
                masks[3] |= UINT16_C(0x0080);
            else
                masks[6] |= UINT16_C(0x0200);
        } else {
            if (p.bank == 1U)
                masks[3] |= UINT16_C(0x0080);
            else
                masks[6] |= (uint16_t)(1U << (p.bank == 2U   ? 9U
                                              : p.bank == 3U ? 10U
                                                             : 12U));
            if (config->banks[i].kind == NX_STM32_FSMC_LCD_NE4)
                masks[6] |= UINT16_C(0x0004);
            else {
                for (size_t bit = p.multiplexed ? p.width : 0U;
                     bit < p.address_lines; ++bit)
                    masks[address[bit][0]] |= (uint16_t)(1U << address[bit][1]);
                if (p.multiplexed)
                    masks[1] |= UINT16_C(0x0080);
                if (p.byte_lanes)
                    masks[4] |= p.width == 16U ? UINT16_C(3) : UINT16_C(1);
                if (p.burst_read)
                    masks[3] |= UINT16_C(8);
            }
        }
        if (p.wait_signal || p.asynchronous_wait)
            masks[3] |= UINT16_C(0x0040);
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
    uint16_t remaining[7];
    required_pins(config, remaining);
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        if (pin->port_index < 1U || pin->port_index > 6U || pin->pin > 15U ||
            (config->package_pins == 100U && pin->port_index >= 5U) ||
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
        if ((remaining[pin->port_index] & bit) == 0U)
            return NX_ERROR_INVALID;
        remaining[pin->port_index] &= (uint16_t)~bit;
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
    return (remaining[0] | remaining[1] | remaining[2] | remaining[3] |
            remaining[4] | remaining[5] | remaining[6]) == 0U
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
            selected |= !nand_bank(bus->config, i) &&
                        bank_index(bus->config, i) == index;
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
    for (uint8_t bank = 2U; bank <= 3U; ++bank) {
        bool selected = false;
        for (size_t i = 0U; i < bus->config->bank_count; ++i)
            selected |= nand_bank(bus->config, i) &&
                        parameters(bus->config, i).bank == bank;
        uint32_t control = NX_STM32_FSMC_READ(
            *(bank == 2U ? &FSMC_Bank2_3->PCR2 : &FSMC_Bank2_3->PCR3));
        uint32_t status = NX_STM32_FSMC_READ(
            *(bank == 2U ? &FSMC_Bank2_3->SR2 : &FSMC_Bank2_3->SR3));
        if (!selected &&
            ((control & FSMC_PCR2_PBKEN) != 0U ||
             (status & (FSMC_SR2_IREN | FSMC_SR2_ILEN | FSMC_SR2_IFEN)) != 0U))
            return true;
    }
    return (NX_STM32_FSMC_READ(FSMC_Bank4->PCR4) & FSMC_PCR4_PBKEN) != 0U ||
           (NX_STM32_FSMC_READ(FSMC_Bank4->SR4) &
            (FSMC_SR4_IREN | FSMC_SR4_ILEN | FSMC_SR4_IFEN)) != 0U;
}

/** \brief Platform owns original snapshots; install only exact AF12 fields. */
static nx_result_t configure_pins(const nx_stm32_fsmc_config_t* config,
                                  bool preserve_boot) {
    for (size_t i = 0U; i < config->bank_count; ++i) {
        if (preserve_boot && config->banks[i].kind == NX_STM32_FSMC_SRAM_NE3)
            continue;
        nx_stm32_fsmc_parameters_t p = parameters(config, i);
        bool pd7 = nand_bank(config, i) ? p.bank == 2U : p.bank == 1U;
        if (pd7)
            GPIOD->BSRR = UINT32_C(1) << 7U;
        else
            GPIOG->BSRR = UINT32_C(1)
                          << (nand_bank(config, i) || p.bank == 2U ? 9U
                              : p.bank == 3U                       ? 10U
                                                                   : 12U);
    }
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
static FSMC_NORSRAM_TimingTypeDef timing(const nx_stm32_fsmc_timing_t* value,
                                         const nx_stm32_fsmc_parameters_t* p) {
    FSMC_NORSRAM_TimingTypeDef result = {value->address_setup,
                                         value->address_hold,
                                         value->data_setup,
                                         value->bus_turn,
                                         p->clock_division,
                                         p->data_latency,
                                         (uint32_t)p->access_mode << 28U};
    return result;
}

/** \brief Independent expected field projection checks actual LL readback. */
static uint32_t timing_bits(const nx_stm32_fsmc_timing_t* value, bool extended,
                            const nx_stm32_fsmc_parameters_t* p) {
    uint32_t result = ((uint32_t)value->address_setup << FSMC_BTR1_ADDSET_Pos) |
                      ((uint32_t)value->address_hold << FSMC_BTR1_ADDHLD_Pos) |
                      ((uint32_t)value->data_setup << FSMC_BTR1_DATAST_Pos) |
                      ((uint32_t)value->bus_turn << FSMC_BTR1_BUSTURN_Pos);
    result |= (uint32_t)p->access_mode << 28U;
    return extended
               ? result
               : result |
                     ((uint32_t)p->clock_division - 1U)
                         << FSMC_BTR1_CLKDIV_Pos |
                     ((uint32_t)p->data_latency - 2U) << FSMC_BTR1_DATLAT_Pos;
}

/** \brief One bank's original values are retained before its first write. */
static nx_result_t configure_bank(nx_stm32_fsmc_t* bus, size_t i) {
    const nx_stm32_fsmc_bank_config_t* config = &bus->config->banks[i];
    nx_stm32_fsmc_bank_state_t* state = &bus->banks[i];
    nx_stm32_fsmc_parameters_t p = parameters(bus->config, i);
    uint32_t index = bank_index(bus->config, i);
    state->saved_control =
        NX_STM32_FSMC_READ(*control_register(bus->config, i));
    state->saved_read = NX_STM32_FSMC_READ(*read_register(bus->config, i));
    state->saved_write = NX_STM32_FSMC_READ(*write_register(bus->config, i));
    state->captured = true;
    if (nand_bank(bus->config, i)) {
#if defined(NEXUS_STM32_FSMC_NAND)
        uint32_t bank = p.bank == 2U ? FSMC_NAND_BANK2 : FSMC_NAND_BANK3;
        uint32_t ecc = 0U;
        for (uint16_t chunk = p.ecc_bytes; chunk > 256U; chunk /= 2U)
            ecc += UINT32_C(1) << FSMC_PCR2_ECCPS_Pos;
        FSMC_NAND_InitTypeDef init = {
            bank,
            p.wait_signal ? FSMC_NAND_PCC_WAIT_FEATURE_ENABLE : 0U,
            p.width == 16U ? FSMC_NAND_PCC_MEM_BUS_WIDTH_16 : 0U,
            0U,
            ecc,
            p.tclr,
            p.tar};
        FSMC_NAND_PCC_TimingTypeDef common = {p.common.setup, p.common.wait,
                                              p.common.hold, p.common.hiz};
        FSMC_NAND_PCC_TimingTypeDef attribute = {
            p.attribute.setup, p.attribute.wait, p.attribute.hold,
            p.attribute.hiz};
        if (FSMC_NAND_Init(FSMC_Bank2_3, &init) != HAL_OK ||
            FSMC_NAND_CommonSpace_Timing_Init(FSMC_Bank2_3, &common, bank) !=
                HAL_OK ||
            FSMC_NAND_AttributeSpace_Timing_Init(FSMC_Bank2_3, &attribute,
                                                 bank) != HAL_OK)
            return NX_ERROR_IO;
        __FSMC_NAND_ENABLE(FSMC_Bank2_3, bank);
        nx_arch_dsb();
        uint32_t expected =
            FSMC_PCR2_PTYP | FSMC_PCR2_PBKEN | ecc |
            (p.width == 16U ? FSMC_NAND_PCC_MEM_BUS_WIDTH_16 : 0U) |
            (p.wait_signal ? FSMC_PCR2_PWAITEN : 0U) |
            ((uint32_t)p.tclr << 9U) | ((uint32_t)p.tar << 13U);
        uint32_t common_bits =
            (uint32_t)p.common.setup | ((uint32_t)p.common.wait << 8U) |
            ((uint32_t)p.common.hold << 16U) | ((uint32_t)p.common.hiz << 24U);
        uint32_t attribute_bits = (uint32_t)p.attribute.setup |
                                  ((uint32_t)p.attribute.wait << 8U) |
                                  ((uint32_t)p.attribute.hold << 16U) |
                                  ((uint32_t)p.attribute.hiz << 24U);
        return NX_STM32_FSMC_READ(*control_register(bus->config, i)) ==
                           expected &&
                       NX_STM32_FSMC_READ(*read_register(bus->config, i)) ==
                           common_bits &&
                       NX_STM32_FSMC_READ(*write_register(bus->config, i)) ==
                           attribute_bits
                   ? NX_SUCCESS
                   : NX_ERROR_IO;
#else
        return NX_ERROR_UNSUPPORTED;
#endif
    }
    FSMC_NORSRAM_InitTypeDef init = {0};
    init.NSBank = index;
    init.DataAddressMux = p.multiplexed ? FSMC_DATA_ADDRESS_MUX_ENABLE : 0U;
    init.MemoryType =
        config->kind == NX_STM32_FSMC_PSRAM ? FSMC_MEMORY_TYPE_PSRAM
        : config->kind == NX_STM32_FSMC_NOR ? FSMC_MEMORY_TYPE_NOR
                                            : FSMC_MEMORY_TYPE_SRAM;
    init.MemoryDataWidth = p.width == 16U ? FSMC_NORSRAM_MEM_BUS_WIDTH_16 : 0U;
    init.BurstAccessMode = p.burst_read ? FSMC_BURST_ACCESS_MODE_ENABLE : 0U;
    init.WaitSignalPolarity = p.wait_high ? FSMC_WAIT_SIGNAL_POLARITY_HIGH : 0U;
    init.WaitSignalActive = p.wait_signal && !p.wait_before
                                ? FSMC_WAIT_TIMING_DURING_WS
                                : FSMC_WAIT_TIMING_BEFORE_WS;
    init.WaitSignal = p.wait_signal ? FSMC_WAIT_SIGNAL_ENABLE : 0U;
    init.AsynchronousWait =
        p.asynchronous_wait ? FSMC_ASYNCHRONOUS_WAIT_ENABLE : 0U;
    init.WriteBurst = p.burst_write ? FSMC_WRITE_BURST_ENABLE : 0U;
    init.WrapMode = p.wrap ? FSMC_WRAP_MODE_ENABLE : 0U;
    init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
    init.ExtendedMode = p.extended_mode ? FSMC_EXTENDED_MODE_ENABLE
                                        : FSMC_EXTENDED_MODE_DISABLE;
    if (p.burst_page_bytes != 0U) {
        uint32_t code = 1U;
        for (uint16_t bytes = p.burst_page_bytes; bytes > 128U; bytes /= 2U)
            ++code;
        init.PageSize = code << 16U;
    }
    FSMC_NORSRAM_TimingTypeDef read = timing(&config->read, &p);
    FSMC_NORSRAM_TimingTypeDef write = timing(&config->write, &p);
    if (FSMC_NORSRAM_Init(FSMC_Bank1, &init) != HAL_OK ||
        FSMC_NORSRAM_Timing_Init(FSMC_Bank1, &read, index) != HAL_OK ||
        FSMC_NORSRAM_Extended_Timing_Init(FSMC_Bank1E, &write, index,
                                          init.ExtendedMode) != HAL_OK)
        return NX_ERROR_IO;
    __FSMC_NORSRAM_ENABLE(FSMC_Bank1, index);
    nx_arch_dsb();
    const uint32_t control_mask =
        (uint32_t)(FSMC_BCR1_MBKEN | FSMC_BCR1_MUXEN | FSMC_BCR1_MTYP |
                   FSMC_BCR1_MWID | FSMC_BCR1_FACCEN | FSMC_BCR1_BURSTEN |
                   FSMC_BCR1_WAITPOL | FSMC_BCR1_WAITCFG | FSMC_BCR1_WRAPMOD |
                   FSMC_BCR1_WREN | FSMC_BCR1_WAITEN | FSMC_BCR1_EXTMOD |
                   FSMC_BCR1_ASYNCWAIT | FSMC_BCR1_CBURSTRW | FSMC_BCR1_CPSIZE);
    const uint32_t expected =
        (uint32_t)FSMC_BCR1_MBKEN | init.DataAddressMux | init.MemoryType |
        init.MemoryDataWidth |
        (config->kind == NX_STM32_FSMC_NOR ? (uint32_t)FSMC_BCR1_FACCEN : 0U) |
        init.BurstAccessMode | init.WaitSignalPolarity | init.WaitSignalActive |
        init.WrapMode | init.WriteOperation | init.WaitSignal |
        init.ExtendedMode | init.AsynchronousWait | init.WriteBurst |
        init.PageSize;
    if ((NX_STM32_FSMC_READ(FSMC_Bank1->BTCR[index]) & control_mask) !=
            expected ||
        NX_STM32_FSMC_READ(FSMC_Bank1->BTCR[index + 1U]) !=
            timing_bits(&config->read, false, &p) ||
        (NX_STM32_FSMC_READ(FSMC_Bank1E->BWTR[index]) &
         (uint32_t)(FSMC_BWTR1_ADDSET | FSMC_BWTR1_ADDHLD | FSMC_BWTR1_DATAST |
                    FSMC_BWTR1_BUSTURN | FSMC_BWTR1_ACCMOD)) !=
            (p.extended_mode ? timing_bits(&config->write, true, &p)
                             : UINT32_C(0x000fffff)))
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
    if (config->bank_count == 0U || config->bank_count > 6U ||
        bank_count != config->bank_count || config->pin_count == 0U ||
        config->pin_count > 64U || config->clock.read == NULL ||
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
    if (config->parameters != NULL &&
        (!valid_span(config->parameters,
                     bank_count * sizeof(*config->parameters),
                     _Alignof(nx_stm32_fsmc_parameters_t)) ||
         !fact_storage(config->parameters,
                       bank_count * sizeof(*config->parameters)) ||
         overlap(config->parameters, bank_count * sizeof(*config->parameters),
                 bus, sizeof(*bus)) ||
         overlap(config->parameters, bank_count * sizeof(*config->parameters),
                 banks, bank_count * sizeof(*banks))))
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
        if (!profile_valid(config, i) ||
            !timing_valid(&config->banks[i].read) ||
            !timing_valid(&config->banks[i].write))
            return NX_ERROR_INVALID;
        for (size_t j = 0U; j < i; ++j) {
            nx_stm32_fsmc_parameters_t left = parameters(config, i);
            nx_stm32_fsmc_parameters_t right = parameters(config, j);
            if ((nand_bank(config, i) == nand_bank(config, j) &&
                 left.bank == right.bank) ||
                (nand_bank(config, i) != nand_bank(config, j) &&
                 (nand_bank(config, i) ? left.bank == right.bank + 1U
                                       : right.bank == left.bank + 1U)))
                return NX_ERROR_INVALID; /* NE1/NCE2 and NE2/NCE3 share pads. */
        }
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
#if !defined(NEXUS_STM32_FSMC_MODEL)
    if (!nx_stm32_clock_profile_matches(RCC, FLASH))
        return NX_ERROR_STATE;
#endif
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
        uint32_t value = NX_STM32_FSMC_READ(*control_register(config, i));
        if ((value & bank_enable(config, i)) != 0U &&
            !(bus->added_gate && !nand_bank(config, i) &&
              bank_index(config, i) == 0U && value == NX_FSMC_RESET_BANK1))
            result = NX_ERROR_BUSY;
        if (nand_bank(config, i) &&
            (NX_STM32_FSMC_READ(*(parameters(config, i).bank == 2U
                                      ? &FSMC_Bank2_3->SR2
                                      : &FSMC_Bank2_3->SR3)) &
             (FSMC_SR2_IREN | FSMC_SR2_ILEN | FSMC_SR2_IFEN)) != 0U)
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
            (NX_STM32_FSMC_READ(*control_register(config, i)) &
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
        volatile uint32_t* control = control_register(bus->config, i);
        volatile uint32_t* read = read_register(bus->config, i);
        volatile uint32_t* write = write_register(bus->config, i);
        *control &= ~bank_enable(bus->config, i);
        nx_arch_dsb();
        if ((NX_STM32_FSMC_READ(*control) & bank_enable(bus->config, i)) != 0U)
            return NX_ERROR_IO;
        *read = state->saved_read;
        *write = state->saved_write;
        *control = state->saved_control;
        nx_arch_dsb();
        if (NX_STM32_FSMC_READ(*control) != state->saved_control ||
            NX_STM32_FSMC_READ(*read) != state->saved_read ||
            NX_STM32_FSMC_READ(*write) != state->saved_write)
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
            (NX_STM32_FSMC_READ(*control_register(bus->config, i)) &
             bank_enable(bus->config, i)) == 0U)
            return NX_ERROR_IO;
        ++bus->epoch;
        *lease =
            (nx_stm32_fsmc_lease_t){bus, bank_address(bus->config, i),
                                    bank_bytes(bus->config, i), bus->epoch};
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

nx_result_t nx_stm32_fsmc_acquire(nx_stm32_fsmc_t* bus, size_t index,
                                  nx_stm32_fsmc_lease_t* lease) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_span(bus, sizeof(*bus), _Alignof(nx_stm32_fsmc_t)) ||
        !control_storage(bus, sizeof(*bus)) ||
        !valid_span(lease, sizeof(*lease), _Alignof(nx_stm32_fsmc_lease_t)) ||
        !control_storage(lease, sizeof(*lease)))
        return NX_ERROR_INVALID;
    if (!bus->prepared || bus->config == NULL)
        return NX_ERROR_STATE;
    if (index >= bus->config->bank_count ||
        !outside_bus(bus, lease, sizeof(*lease)))
        return NX_ERROR_INVALID;
    if (bus->boot_owned &&
        bus->config->banks[index].kind == NX_STM32_FSMC_SRAM_NE3)
        return NX_ERROR_UNSUPPORTED;
    if (lease->owner != NULL || bus->banks[index].lease != NULL)
        return NX_ERROR_BUSY;
    if (bus->epoch == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    if (!clock_present() ||
        (NX_STM32_FSMC_READ(*control_register(bus->config, index)) &
         bank_enable(bus->config, index)) == 0U)
        return NX_ERROR_IO;
    ++bus->epoch;
    *lease =
        (nx_stm32_fsmc_lease_t){bus, bank_address(bus->config, index),
                                bank_bytes(bus->config, index), bus->epoch};
    bus->banks[index].lease = lease;
    return NX_SUCCESS;
}
nx_result_t nx_stm32_fsmc_check(const nx_stm32_fsmc_lease_t* lease) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_span(lease, sizeof(*lease), _Alignof(nx_stm32_fsmc_lease_t)) ||
        !control_storage(lease, sizeof(*lease)) || lease->owner == NULL)
        return NX_ERROR_INVALID;
    const nx_stm32_fsmc_t* bus = lease->owner;
    if (!valid_span(bus, sizeof(*bus), _Alignof(nx_stm32_fsmc_t)) ||
        !control_storage(bus, sizeof(*bus)) || !bus->prepared ||
        bus->config == NULL)
        return NX_ERROR_STATE;
    for (size_t i = 0U; i < bus->config->bank_count; ++i) {
        if (bus->banks[i].lease != lease)
            continue;
        if (lease->epoch == 0U ||
            lease->address != bank_address(bus->config, i) ||
            lease->bytes != bank_bytes(bus->config, i))
            return NX_ERROR_STATE;
        return clock_present() &&
                       (NX_STM32_FSMC_READ(*control_register(bus->config, i)) &
                        bank_enable(bus->config, i)) != 0U
                   ? NX_SUCCESS
                   : NX_ERROR_IO;
    }
    return NX_ERROR_STATE;
}
/** \brief Generic PSRAM geometry is an immutable selected bank projection. */
static nx_result_t generic_geometry(const void* context,
                                    nx_memory_window_geometry_t* info) {
    const nx_stm32_fsmc_window_t* window = context;
    if (window == NULL || window->bus == NULL || window->bus->config == NULL ||
        window->index >= window->bus->config->bank_count || info == NULL)
        return NX_ERROR_STATE;
    if (!nx_stm32_fsmc_storage(window->bus, info, sizeof(*info), true) ||
        overlap(window, sizeof(*window), info, sizeof(*info)))
        return NX_ERROR_INVALID;
    const nx_stm32_fsmc_config_t* config = window->bus->config;
    if (config->banks[window->index].kind != NX_STM32_FSMC_PSRAM)
        return NX_ERROR_UNSUPPORTED;
    *info =
        (nx_memory_window_geometry_t){bank_address(config, window->index),
                                      bank_bytes(config, window->index), false};
    return NX_SUCCESS;
}
static nx_result_t generic_acquire(void* context,
                                   nx_memory_window_lease_t* lease) {
    const nx_stm32_fsmc_window_t* window = context;
    return window == NULL
               ? NX_ERROR_INVALID
               : nx_stm32_fsmc_acquire(window->bus, window->index, lease);
}
static nx_result_t generic_release(void* context,
                                   nx_memory_window_lease_t* lease,
                                   uint64_t epoch) {
    const nx_stm32_fsmc_window_t* window = context;
    if (window == NULL || window->bus == NULL || lease == NULL ||
        window->bus->config == NULL ||
        window->index >= window->bus->config->bank_count ||
        window->bus->banks[window->index].lease != lease)
        return NX_ERROR_INVALID;
    return nx_stm32_fsmc_release(lease, epoch);
}
const nx_memory_window_ops_t nx_stm32_fsmc_window_ops = {
    generic_acquire, generic_release, generic_geometry};

bool nx_stm32_fsmc_storage(const nx_stm32_fsmc_t* bus, const void* storage,
                           size_t bytes, bool writable) {
    if (bus == NULL || bus->config == NULL || !bus->prepared || bytes == 0U ||
        !valid_span(storage, bytes, 1U) ||
        !(writable ? control_storage(storage, bytes)
                   : fact_storage(storage, bytes)))
        return false;
    return outside_bus(bus, storage, bytes);
}
