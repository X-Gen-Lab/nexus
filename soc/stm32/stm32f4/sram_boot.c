/**
 * \file            sram_boot.c
 * \brief           Reset-only internal-Flash and MSP external SRAM bootstrap
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "private/stm32f407_fsmc_profile.h"
#include "private/stm32f407_sram_boot.h"
#include "stm32f4xx_ll_fsmc.h"
#include "stm32f4xx_ll_gpio.h"
#ifndef NX_SRAM_BOOT_READ
#define NX_SRAM_BOOT_READ(p)         READ_REG(*(p))
#define NX_SRAM_BOOT_WRITE(p, value) WRITE_REG(*(p), (value))
#endif
#ifndef NX_SRAM_BOOT_BARRIER
#define NX_SRAM_BOOT_BARRIER() __DSB()
#endif
#ifndef NX_SRAM_BOOT_MSP
#define NX_SRAM_BOOT_MSP()       __get_MSP()
#define NX_SRAM_BOOT_CONTROL()   __get_CONTROL()
#define NX_SRAM_BOOT_IPSR()      __get_IPSR()
#define NX_SRAM_BOOT_PRIMASK()   __get_PRIMASK()
#define NX_SRAM_BOOT_BASEPRI()   __get_BASEPRI()
#define NX_SRAM_BOOT_FAULTMASK() __get_FAULTMASK()
#endif
#ifndef NX_SRAM_BOOT_DEVICE_ID
#define NX_SRAM_BOOT_DEVICE_ID() NX_SRAM_BOOT_READ(&DBGMCU->IDCODE)
#define NX_SRAM_BOOT_FLASH_KIB()                                               \
    (*(const volatile uint16_t*)(uintptr_t)FLASHSIZE_BASE)
#endif
#define GPIO_CLOCKS                                                            \
    (RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN | RCC_AHB1ENR_GPIOFEN |         \
     RCC_AHB1ENR_GPIOGEN)
#define BANK_CONTROL                                                           \
    (FSMC_BCR1_MBKEN | FSMC_NORSRAM_MEM_BUS_WIDTH_16 |                         \
     FSMC_WRITE_OPERATION_ENABLE | FSMC_EXTENDED_MODE_ENABLE)
#define BANK_CONTROL_MASK                                                      \
    (FSMC_BCR1_MBKEN | FSMC_BCR1_MUXEN | FSMC_BCR1_MTYP | FSMC_BCR1_MWID |     \
     FSMC_BCR1_FACCEN | FSMC_BCR1_BURSTEN | FSMC_BCR1_WAITPOL |                \
     FSMC_BCR1_WRAPMOD | FSMC_BCR1_WAITCFG | FSMC_BCR1_WREN |                  \
     FSMC_BCR1_WAITEN | FSMC_BCR1_EXTMOD | FSMC_BCR1_ASYNCWAIT |               \
     FSMC_BCR1_CPSIZE | FSMC_BCR1_CBURSTRW)
#define WRITE_TIMING_MASK                                                      \
    (FSMC_BWTR1_ADDSET | FSMC_BWTR1_ADDHLD | FSMC_BWTR1_DATAST |               \
     FSMC_BWTR1_BUSTURN | FSMC_BWTR1_ACCMOD)
_Static_assert(FSMC_NORSRAM_BANK3 == 4U && LL_GPIO_AF_12 == 12U &&
                   FSMC_NORSRAM_MEM_BUS_WIDTH_16 == FSMC_BCR1_MWID_0,
               "F407 SRAM bootstrap hardware profile changed");
/** \brief A complete immutable object must be inside physical internal Flash.
 */
static bool flash_object(const void* p, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)p;
    return address >= UINT32_C(0x08000000) && address % alignment == 0U &&
           address <= UINT32_C(0x08100000) &&
           bytes <= UINT32_C(0x08100000) - address;
}
static bool timing_valid(const nx_stm32_fsmc_timing_t* t) {
    return t->address_setup <= 15U && t->address_hold >= 1U &&
           t->address_hold <= 15U && t->data_setup >= 1U && t->bus_turn <= 15U;
}
bool nx_stm32_sram_boot_plan_valid(const nx_stm32_sram_boot_plan_t* p) {
    if (!flash_object(p, sizeof(*p), _Alignof(nx_stm32_sram_boot_plan_t)) ||
        p->version != NX_STM32_SRAM_BOOT_VERSION ||
        p->part != NX_STM32_SRAM_IS62WV51216 ||
        p->maximum_hclk_hz != 168000000U ||
        (p->region_count != 2U && p->region_count != 3U) ||
        p->flash_start != UINT32_C(0x08000000) ||
        p->flash_end != UINT32_C(0x08100000) ||
        p->vector_address != p->flash_start ||
        p->msp_start < UINT32_C(0x20000000) || p->msp_start >= p->msp_end ||
        p->msp_end != UINT32_C(0x20020000) ||
        ((p->msp_start | p->msp_end) & 7U) != 0U ||
        p->msp_end - p->msp_start < NX_STM32_SRAM_BOOT_MIN_STACK ||
        !flash_object(p->bank, sizeof(*p->bank),
                      _Alignof(nx_stm32_fsmc_bank_config_t)))
        return false;
    const nx_stm32_memory_region_t* first = &p->regions[0];
    const nx_stm32_memory_region_t* last = &p->regions[p->region_count - 1U];
    if (first->start != UINT32_C(0x20000000) ||
        first->end != UINT32_C(0x20020000) ||
        last->start != UINT32_C(0x68000000) ||
        last->end != UINT32_C(0x68100000) ||
        p->bank->kind != NX_STM32_FSMC_SRAM_NE3 ||
        !timing_valid(&p->bank->read) || !timing_valid(&p->bank->write))
        return false;
    if (p->region_count == 3U && (p->regions[1].start != UINT32_C(0x10000000) ||
                                  p->regions[1].end != UINT32_C(0x10010000)))
        return false;
    return nx_stm32_memory_plan_valid(p->regions, p->region_count,
                                      p->flash_start, p->flash_end,
                                      p->msp_start, p->msp_end);
}
/** \brief Die/density can be observed; package/PCB identity stays authored. */
static bool device_matches(void) {
    return (NX_SRAM_BOOT_DEVICE_ID() & DBGMCU_IDCODE_DEV_ID) == 0x413U &&
           NX_SRAM_BOOT_FLASH_KIB() == 1024U;
}
/** \brief Reset entry is a contract; register equality cannot prove provenance.
 */
static bool reset_context(const nx_stm32_sram_boot_plan_t* p) {
    uintptr_t msp = NX_SRAM_BOOT_MSP();
    if (msp < p->msp_start || msp > p->msp_end || (msp & 7U) ||
        NX_SRAM_BOOT_CONTROL() || NX_SRAM_BOOT_IPSR() ||
        NX_SRAM_BOOT_PRIMASK() || NX_SRAM_BOOT_BASEPRI() ||
        NX_SRAM_BOOT_FAULTMASK() || NX_SRAM_BOOT_READ(&SCB->VTOR) != 0U ||
        (NX_SRAM_BOOT_READ(&SysTick->CTRL) & SysTick_CTRL_ENABLE_Msk) ||
        (NX_SRAM_BOOT_READ(&SCB->ICSR) &
         (SCB_ICSR_PENDSTSET_Msk | SCB_ICSR_PENDSVSET_Msk)))
        return false;
    for (size_t i = 0U; i < 3U; ++i) {
        if (NX_SRAM_BOOT_READ(&NVIC->ISER[i]) ||
            NX_SRAM_BOOT_READ(&NVIC->ISPR[i]) ||
            NX_SRAM_BOOT_READ(&NVIC->IABR[i]))
            return false;
    }
    uint32_t clock = NX_SRAM_BOOT_READ(&RCC->CR);
    uint32_t forbidden =
        (uint32_t)(RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_HSEBYP | RCC_CR_CSSON |
                   RCC_CR_PLLON | RCC_CR_PLLRDY | RCC_CR_PLLI2SON |
                   RCC_CR_PLLI2SRDY);
    return (clock & (RCC_CR_HSION | RCC_CR_HSIRDY)) ==
               (RCC_CR_HSION | RCC_CR_HSIRDY) &&
           (clock & RCC_CR_HSITRIM) == (16U << RCC_CR_HSITRIM_Pos) &&
           !(clock & forbidden) && NX_SRAM_BOOT_READ(&RCC->CFGR) == 0U &&
           NX_SRAM_BOOT_READ(&RCC->PLLCFGR) == UINT32_C(0x24003010) &&
           NX_SRAM_BOOT_READ(&RCC->SSCGR) == 0U &&
           NX_SRAM_BOOT_READ(&RCC->AHB1ENR) == RCC_AHB1ENR_CCMDATARAMEN &&
           NX_SRAM_BOOT_READ(&RCC->AHB2ENR) == 0U &&
           NX_SRAM_BOOT_READ(&RCC->AHB3ENR) == 0U &&
           NX_SRAM_BOOT_READ(&RCC->APB1ENR) == 0U &&
           NX_SRAM_BOOT_READ(&RCC->APB2ENR) == 0U &&
           NX_SRAM_BOOT_READ(&RCC->AHB1RSTR) == 0U &&
           NX_SRAM_BOOT_READ(&RCC->AHB2RSTR) == 0U &&
           NX_SRAM_BOOT_READ(&RCC->AHB3RSTR) == 0U &&
           NX_SRAM_BOOT_READ(&RCC->APB1RSTR) == 0U &&
           NX_SRAM_BOOT_READ(&RCC->APB2RSTR) == 0U;
}
/** \brief Readonly Flash constants describe the exact reviewed 40-pad route. */
static uint16_t pad_mask(unsigned bank) {
    switch (bank) {
        case 0U:
            return UINT16_C(0xff33);
        case 1U:
            return UINT16_C(0xff83);
        case 2U:
            return UINT16_C(0xf03f);
        default:
            return UINT16_C(0x043f);
    }
}
static GPIO_TypeDef* pad_bank(unsigned bank) {
    return (GPIO_TypeDef*)(uintptr_t)(GPIOD_BASE +
                                      (uintptr_t)bank * UINT32_C(0x400));
}
static bool gates_present(void) {
    return (NX_SRAM_BOOT_READ(&RCC->AHB1ENR) & GPIO_CLOCKS) == GPIO_CLOCKS &&
           (NX_SRAM_BOOT_READ(&RCC->AHB3ENR) & RCC_AHB3ENR_FSMCEN) != 0U;
}
/** \brief Powered observations precede all pad/bank changes. */
static bool reset_pads_and_banks(void) {
    for (unsigned bank = 0U; bank < 4U; ++bank) {
        GPIO_TypeDef* r = pad_bank(bank);
        uint16_t mask = pad_mask(bank);
        if (NX_SRAM_BOOT_READ(&r->LCKR) & mask)
            return false;
        for (unsigned pin = 0U; pin < 16U; ++pin) {
            if (!(mask & (1U << pin)))
                continue;
            uint32_t shift = pin * 2U;
            uint32_t af_shift = (pin % 8U) * 4U;
            if (((NX_SRAM_BOOT_READ(&r->MODER) >> shift) & 3U) ||
                ((NX_SRAM_BOOT_READ(&r->OTYPER) >> pin) & 1U) ||
                ((NX_SRAM_BOOT_READ(&r->PUPDR) >> shift) & 3U) ||
                ((NX_SRAM_BOOT_READ(&r->OSPEEDR) >> shift) & 3U) ||
                ((NX_SRAM_BOOT_READ(&r->AFR[pin / 8U]) >> af_shift) & 15U))
                return false;
        }
        uint32_t control = bank ? UINT32_C(0x30d2) : UINT32_C(0x30db);
        if ((NX_SRAM_BOOT_READ(&FSMC_Bank1->BTCR[(size_t)bank * 2U]) &
             BANK_CONTROL_MASK) != (control & BANK_CONTROL_MASK) ||
            NX_SRAM_BOOT_READ(&FSMC_Bank1->BTCR[(size_t)bank * 2U + 1U]) !=
                UINT32_C(0x0fffffff) ||
            (NX_SRAM_BOOT_READ(&FSMC_Bank1E->BWTR[(size_t)bank * 2U]) &
             WRITE_TIMING_MASK) != (UINT32_C(0x0fffffff) & WRITE_TIMING_MASK))
            return false;
    }
    return true;
}
static void configure_pin(GPIO_TypeDef* r, unsigned pin) {
    uint32_t bit = 1U << pin;
    LL_GPIO_SetPinOutputType(r, bit, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinPull(r, bit, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinSpeed(r, bit, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    if (pin < 8U)
        LL_GPIO_SetAFPin_0_7(r, bit, LL_GPIO_AF_12);
    else
        LL_GPIO_SetAFPin_8_15(r, bit, LL_GPIO_AF_12);
    LL_GPIO_SetPinMode(r, bit, LL_GPIO_MODE_ALTERNATE);
}
static FSMC_NORSRAM_TimingTypeDef timing(const nx_stm32_fsmc_timing_t* t) {
    FSMC_NORSRAM_TimingTypeDef result = {
        t->address_setup,  t->address_hold, t->data_setup, t->bus_turn, 2U, 2U,
        FSMC_ACCESS_MODE_A};
    return result;
}
static uint32_t timing_bits(const nx_stm32_fsmc_timing_t* t, bool extended) {
    uint32_t bits =
        (uint32_t)t->address_setup | (uint32_t)t->address_hold << 4U |
        (uint32_t)t->data_setup << 8U | (uint32_t)t->bus_turn << 16U;
    return extended ? bits : bits | (UINT32_C(1) << FSMC_BTR1_CLKDIV_Pos);
}
bool nx_stm32_sram_boot_matches(const nx_stm32_sram_boot_plan_t* p) {
    if (!nx_stm32_sram_boot_plan_valid(p) || !device_matches() ||
        !gates_present() ||
        (NX_SRAM_BOOT_READ(&FSMC_Bank1->BTCR[4]) & BANK_CONTROL_MASK) !=
            BANK_CONTROL ||
        NX_SRAM_BOOT_READ(&FSMC_Bank1->BTCR[5]) !=
            timing_bits(&p->bank->read, false) ||
        (NX_SRAM_BOOT_READ(&FSMC_Bank1E->BWTR[4]) & WRITE_TIMING_MASK) !=
            timing_bits(&p->bank->write, true))
        return false;
    for (unsigned bank = 0U; bank < 4U; ++bank) {
        GPIO_TypeDef* r = pad_bank(bank);
        uint16_t mask = pad_mask(bank);
        if (NX_SRAM_BOOT_READ(&r->LCKR) & mask)
            return false;
        for (unsigned pin = 0U; pin < 16U; ++pin) {
            if (!(mask & (1U << pin)))
                continue;
            uint32_t shift = pin * 2U;
            uint32_t af_shift = (pin % 8U) * 4U;
            if (((NX_SRAM_BOOT_READ(&r->MODER) >> shift) & 3U) !=
                    LL_GPIO_MODE_ALTERNATE ||
                ((NX_SRAM_BOOT_READ(&r->OTYPER) >> pin) & 1U) ||
                ((NX_SRAM_BOOT_READ(&r->PUPDR) >> shift) & 3U) ||
                ((NX_SRAM_BOOT_READ(&r->OSPEEDR) >> shift) & 3U) !=
                    LL_GPIO_SPEED_FREQ_VERY_HIGH ||
                ((NX_SRAM_BOOT_READ(&r->AFR[pin / 8U]) >> af_shift) & 15U) !=
                    LL_GPIO_AF_12)
                return false;
        }
    }
    return gates_present();
}
bool nx_stm32_sram_bootstrap(const nx_stm32_sram_boot_plan_t* p) {
    if (!nx_stm32_sram_boot_plan_valid(p) || !device_matches() ||
        !reset_context(p))
        return false;
    NX_SRAM_BOOT_WRITE(&RCC->AHB1ENR,
                       NX_SRAM_BOOT_READ(&RCC->AHB1ENR) | GPIO_CLOCKS);
    NX_SRAM_BOOT_WRITE(&RCC->AHB3ENR, RCC_AHB3ENR_FSMCEN);
    NX_SRAM_BOOT_BARRIER();
    if (!gates_present() || !reset_pads_and_banks())
        return false;
    /* NE3 is first driven inactive as GPIO while its decoder remains off.
     * Other pads enter AF12 before CS, preventing partial-address writes. */
    LL_GPIO_SetOutputPin(GPIOG, 1U << 10U);
    LL_GPIO_SetPinOutputType(GPIOG, 1U << 10U, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinPull(GPIOG, 1U << 10U, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinMode(GPIOG, 1U << 10U, LL_GPIO_MODE_OUTPUT);
    for (unsigned bank = 0U; bank < 4U; ++bank) {
        uint16_t mask = pad_mask(bank);
        for (unsigned pin = 0U; pin < 16U; ++pin) {
            if ((mask & (1U << pin)) && !(bank == 3U && pin == 10U))
                configure_pin(pad_bank(bank), pin);
        }
    }
    /* Constant hardware personality stays in Flash. A zero-initialized local
     * SDK record can introduce compiler memset before the C runtime exists. */
    static const FSMC_NORSRAM_InitTypeDef init = {
        .NSBank = FSMC_NORSRAM_BANK3,
        .MemoryType = FSMC_MEMORY_TYPE_SRAM,
        .MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16,
        .WriteOperation = FSMC_WRITE_OPERATION_ENABLE,
        .ExtendedMode = FSMC_EXTENDED_MODE_ENABLE};
    FSMC_NORSRAM_TimingTypeDef read = timing(&p->bank->read);
    FSMC_NORSRAM_TimingTypeDef write = timing(&p->bank->write);
    if (FSMC_NORSRAM_Init(FSMC_Bank1, &init) != HAL_OK ||
        FSMC_NORSRAM_Timing_Init(FSMC_Bank1, &read, FSMC_NORSRAM_BANK3) !=
            HAL_OK ||
        FSMC_NORSRAM_Extended_Timing_Init(FSMC_Bank1E, &write,
                                          FSMC_NORSRAM_BANK3,
                                          FSMC_EXTENDED_MODE_ENABLE) != HAL_OK)
        return false;
    configure_pin(GPIOG, 10U);
    __FSMC_NORSRAM_ENABLE(FSMC_Bank1, FSMC_NORSRAM_BANK3);
    NX_SRAM_BOOT_BARRIER();
    if (!nx_stm32_sram_boot_matches(p))
        return false;
    return nx_stm32_memory_initialize(&p->regions[p->region_count - 1U],
                                      p->flash_start, p->flash_end);
}
