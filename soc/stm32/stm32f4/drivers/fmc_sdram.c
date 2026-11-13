/**
 * \file            fmc_sdram.c
 * \brief           Official FMC commands and retained finite cold progress
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
#include "stm32f4_sdram.h"
#include "stm32f4xx_ll_fmc.h"
#include "stm32f4xx_ll_gpio.h"

#if !defined(STM32F479xx) && !defined(STM32F446xx)
#error "This SDRAM profile requires the reviewed F479/F446 controller"
#endif

extern nx_stm32_system_t g_nx_stm32_system;

enum {
    CONFIGURED = 1,
    CLOCK_PENDING,
    POWERUP_DELAY,
    PALL_PENDING,
    REFRESH_PENDING,
    MODE_PENDING,
    REFRESH_WRITE,
    READY,
    SELF_PENDING,
    QUIESCENT
};

/** \brief Entire metadata/control spans remain internal through cold exit. */
static bool span(const void* object, size_t bytes, size_t alignment,
                 bool write) {
    uintptr_t address = (uintptr_t)object;
    if (object == NULL || address % alignment != 0U ||
        bytes > UINTPTR_MAX - address)
        return false;
    uint64_t first = address, end = first + bytes;
    return !(first < UINT64_C(0x100000000) && UINT64_C(0x40000000) < end) &&
           (!write ||
            !(first < UINT64_C(0x10000000) && UINT64_C(0x08000000) < end));
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
static bool context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool owner_valid(const nx_stm32_sdram_t* owner) {
    return span(owner, sizeof(*owner), _Alignof(nx_stm32_sdram_t), true);
}
/** \brief A matching frequency never rebinds an owner to a new time domain. */
static bool system_live(const nx_stm32_sdram_config_t* config) {
    const nx_stm32_system_t* system = config->system;
    return system == &g_nx_stm32_system && system->started &&
           system->timer_error == NX_SUCCESS && system->time_epoch != 0U &&
           system->core_hz == NEXUS_CORE_HZ && system->rcc == RCC &&
           system->flash == FLASH && system->power == PWR &&
           system->timer == TIM2;
}
static bool epoch_live(const nx_stm32_sdram_t* owner) {
    return system_live(owner->config) &&
           owner->system_epoch == owner->config->system->time_epoch;
}
static nx_result_t clock_read(nx_stm32_sdram_t* owner, nx_time_us_t* now) {
    if (!epoch_live(owner))
        return NX_ERROR_STATE;
    *now = nx_stm32_system_clock_read(owner->config->system);
    if (!epoch_live(owner) || *now == NX_DEADLINE_NEVER ||
        *now < owner->last_clock)
        return NX_ERROR_STATE;
    owner->last_clock = *now;
    return NX_SUCCESS;
}
static bool clock_matches(const nx_stm32_sdram_config_t* config) {
    return config->hclk_hz == NEXUS_CORE_HZ &&
           nx_stm32_clock_profile_matches(RCC, FLASH);
}

/** \brief A software CPU loan never silently rewrites a global memory swap. */
static bool mapping_matches(void) {
    return (READ_REG(RCC->APB2ENR) & RCC_APB2ENR_SYSCFGEN) != 0U &&
           (READ_REG(SYSCFG->MEMRMP) & SYSCFG_MEMRMP_SWP_FMC) == 0U;
}
/** \brief ES0321 2.3.4: the shared Write FIFO must remain enabled.
 * \note FMCEN0 cannot provide trustworthy BCR1 readback. Never take over or
 *       rewrite a foreign WFDIS bit to manufacture this physical constraint.
 */
static bool write_fifo_enabled(void) {
    if ((READ_REG(RCC->AHB3ENR) & RCC_AHB3ENR_FMCEN) == 0U)
        return false;
    return (READ_REG(FMC_Bank1->BTCR[0]) & FMC_BCR1_WFDIS) == 0U;
}

/** \brief This first reviewed route is Bank1, 16-bit, row12/column8. */
static nx_result_t pins_valid(const nx_stm32_sdram_t* owner,
                              const nx_stm32_sdram_config_t* config,
                              bool inspect_hardware) {
    uint16_t remaining[] = {0U, 0U, 0xdU, 0xc703U, 0xff83U, 0xf83fU, 0x8133U};
    if (config->pin_count != 38U ||
        !span(config->pins, config->pin_count * sizeof(*config->pins),
              _Alignof(nx_stm32_platform_pin_config_t), false) ||
        overlap(owner, sizeof(*owner), config->pins,
                config->pin_count * sizeof(*config->pins)) ||
        overlap(config->pins, config->pin_count * sizeof(*config->pins),
                config->system, sizeof(*config->system)))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        if (pin->port_index >= 7U || pin->pin >= 16U ||
            pin->registers !=
                (GPIO_TypeDef*)(GPIOA_BASE +
                                (uintptr_t)pin->port_index * UINT32_C(0x400)) ||
            !span(pin->snapshot, sizeof(*pin->snapshot), _Alignof(uint16_t),
                  true))
            return NX_ERROR_INVALID;
        uint16_t selected = (uint16_t)(1U << pin->pin);
        if ((remaining[pin->port_index] & selected) == 0U ||
            overlap(pin->snapshot, sizeof(*pin->snapshot), owner,
                    sizeof(*owner)) ||
            overlap(pin->snapshot, sizeof(*pin->snapshot), config,
                    sizeof(*config)) ||
            overlap(pin->snapshot, sizeof(*pin->snapshot), config->pins,
                    config->pin_count * sizeof(*config->pins)) ||
            overlap(pin->snapshot, sizeof(*pin->snapshot), config->system,
                    sizeof(*config->system)))
            return NX_ERROR_INVALID;
        remaining[pin->port_index] &= (uint16_t)~selected;
        for (size_t j = 0U; j < i; ++j)
            if (pin->snapshot == config->pins[j].snapshot)
                return NX_ERROR_INVALID;
        if (!inspect_hardware)
            continue;
        if ((READ_REG(RCC->AHB1ENR) & (UINT32_C(1) << pin->port_index)) == 0U)
            return NX_ERROR_STATE;
        if ((READ_REG(pin->registers->LCKR) &
             ((uint32_t)selected | GPIO_LCKR_LCKK)) ==
            ((uint32_t)selected | GPIO_LCKR_LCKK))
            return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}

/** \brief Rational HCLK/SDCLK conversion rounds every device minimum upward. */
static bool timing_cycles(nx_stm32_sdram_minimum_t minimum, uint32_t hclk,
                          uint8_t divide, uint32_t* result) {
    uint64_t denominator = UINT64_C(1000000000) * divide;
    uint64_t cycles =
        ((uint64_t)minimum.nanoseconds * hclk + denominator - 1U) / denominator;
    if (cycles < minimum.cycles)
        cycles = minimum.cycles;
    if (cycles == 0U || cycles > 16U)
        return false;
    *result = (uint32_t)cycles;
    return true;
}
static bool plan(const nx_stm32_sdram_config_t* config,
                 FMC_SDRAM_TimingTypeDef* timing, uint32_t* refresh) {
    if (config->row_bits != 12U || config->column_bits != 8U ||
        config->internal_banks != 4U || config->width_bits != 16U ||
        config->bytes != 0x800000U || config->cas < 1U || config->cas > 3U ||
        (config->sdclk_div != 2U && config->sdclk_div != 3U) ||
        config->read_pipe > 2U || config->initial_refreshes == 0U ||
        config->initial_refreshes > 16U || config->refresh_window_us == 0U ||
        config->powerup_us == 0U || config->powerup_us > 1000000U ||
        config->prepare_budget_us == 0U ||
        config->prepare_budget_us > 1000000U || config->stop_budget_us == 0U ||
        config->stop_budget_us > 1000000U || config->hclk_hz == 0U ||
        config->hclk_hz > 180000000U)
        return false;
    const nx_stm32_sdram_timing_t* selected = &config->timing;
    if (!timing_cycles(selected->mrd, config->hclk_hz, config->sdclk_div,
                       &timing->LoadToActiveDelay) ||
        !timing_cycles(selected->xsr, config->hclk_hz, config->sdclk_div,
                       &timing->ExitSelfRefreshDelay) ||
        !timing_cycles(selected->ras, config->hclk_hz, config->sdclk_div,
                       &timing->SelfRefreshTime) ||
        !timing_cycles(selected->rc, config->hclk_hz, config->sdclk_div,
                       &timing->RowCycleDelay) ||
        !timing_cycles(selected->wr, config->hclk_hz, config->sdclk_div,
                       &timing->WriteRecoveryTime) ||
        !timing_cycles(selected->rp, config->hclk_hz, config->sdclk_div,
                       &timing->RPDelay) ||
        !timing_cycles(selected->rcd, config->hclk_hz, config->sdclk_div,
                       &timing->RCDDelay))
        return false;
    uint32_t rc = timing->SelfRefreshTime + timing->RPDelay;
    if (timing->RowCycleDelay < rc)
        timing->RowCycleDelay = rc;
    uint32_t wr = timing->SelfRefreshTime > timing->RCDDelay
                      ? timing->SelfRefreshTime - timing->RCDDelay
                      : 0U;
    if (timing->WriteRecoveryTime < wr)
        timing->WriteRecoveryTime = wr;
    uint32_t subtract = timing->RCDDelay + timing->RPDelay;
    wr = timing->RowCycleDelay > subtract ? timing->RowCycleDelay - subtract
                                          : 0U;
    if (timing->WriteRecoveryTime < wr)
        timing->WriteRecoveryTime = wr;
    if (timing->RowCycleDelay > 16U || timing->WriteRecoveryTime > 16U)
        return false;
    uint64_t denominator = (UINT64_C(1) << config->row_bits) *
                           config->sdclk_div * UINT64_C(1000000);
    uint64_t count =
        (uint64_t)config->refresh_window_us * config->hclk_hz / denominator;
    if (count < 61U || count > 8211U)
        return false;
    count -= 20U;
    if (count == timing->WriteRecoveryTime + timing->RPDelay +
                     timing->RowCycleDelay + timing->RCDDelay + 4U) {
        if (count == 41U)
            return false;
        --count; /* Earlier refresh stays within the device's maximum interval.
                  */
    }
    *refresh = (uint32_t)count;
    return true;
}

static nx_result_t configure_pins(const nx_stm32_sdram_config_t* config) {
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
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
        if (((READ_REG(pin->registers->MODER) >> shift) & 3U) !=
                LL_GPIO_MODE_ALTERNATE ||
            ((READ_REG(pin->registers->OTYPER) >> pin->pin) & 1U) != 0U ||
            ((READ_REG(pin->registers->PUPDR) >> shift) & 3U) != 0U ||
            ((READ_REG(pin->registers->OSPEEDR) >> shift) & 3U) !=
                LL_GPIO_SPEED_FREQ_VERY_HIGH ||
            ((READ_REG(pin->registers->AFR[pin->pin / 8U]) >>
              ((uint32_t)pin->pin % 8U * 4U)) &
             15U) != 12U)
            return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

/** \brief LL may return TIMEOUT immediately; only C observes actual completion.
 */
static nx_result_t command(nx_stm32_sdram_t* owner, uint32_t mode,
                           uint8_t phase, nx_time_us_t deadline) {
    nx_time_us_t now;
    nx_result_t result = clock_read(owner, &now);
    if (result != NX_SUCCESS || now >= deadline)
        return result != NX_SUCCESS ? result : NX_ERROR_TIMEOUT;
    if (!write_fifo_enabled())
        return NX_ERROR_UNSUPPORTED;
    if ((READ_REG(FMC_Bank5_6->SDSR) & FMC_SDSR_BUSY) != 0U)
        return NX_ERROR_BUSY;
    FMC_SDRAM_CommandTypeDef selected = {
        mode, FMC_SDRAM_CMD_TARGET_BANK1,
        mode == FMC_SDRAM_CMD_AUTOREFRESH_MODE
            ? owner->config->initial_refreshes
            : 1U,
        mode == FMC_SDRAM_CMD_LOAD_MODE ? (uint32_t)owner->config->cas << 4U
                                        : 0U};
    owner->phase = phase; /* The following write may publish a real command. */
    (void)FMC_SDRAM_SendCommand(FMC_Bank5_6, &selected, 0U);
    nx_arch_dsb();
    /* MODE/CTB are write-only (RM0386 p365), never readback evidence. */
    uint32_t expected =
        ((selected.AutoRefreshNumber - 1U) << FMC_SDCMR_NRFS_Pos) |
        (selected.ModeRegisterDefinition << FMC_SDCMR_MRD_Pos);
    return (READ_REG(FMC_Bank5_6->SDCMR) & (FMC_SDCMR_NRFS | FMC_SDCMR_MRD)) ==
                   expected
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
static nx_result_t wait_ready(nx_stm32_sdram_t* owner, nx_time_us_t deadline) {
    for (;;) {
        nx_time_us_t now;
        nx_result_t result = clock_read(owner, &now);
        if (result != NX_SUCCESS)
            return result;
        uint32_t expected =
            owner->phase == REFRESH_PENDING
                ? ((uint32_t)owner->config->initial_refreshes - 1U)
                      << FMC_SDCMR_NRFS_Pos
            : owner->phase == MODE_PENDING
                ? (uint32_t)owner->config->cas << (FMC_SDCMR_MRD_Pos + 4U)
                : 0U;
        if ((READ_REG(FMC_Bank5_6->SDCMR) & (FMC_SDCMR_NRFS | FMC_SDCMR_MRD)) !=
            expected)
            return NX_ERROR_IO;
        uint32_t status = READ_REG(FMC_Bank5_6->SDSR);
        if ((status & FMC_SDSR_BUSY) == 0U) {
            uint32_t mode =
                owner->phase == SELF_PENDING ? FMC_SDSR_MODES1_0 : 0U;
            return (status &
                    (FMC_SDSR_RE | FMC_SDSR_MODES1 | FMC_SDSR_MODES2)) == mode
                       ? NX_SUCCESS
                       : NX_ERROR_IO;
        }
        if (now >= deadline)
            return NX_ERROR_TIMEOUT;
    }
}
/** \brief Resume only completed cold phases; never replay an uncertain command.
 */
static nx_result_t initialize(nx_stm32_sdram_t* owner, nx_time_us_t deadline) {
    while (owner->phase < READY) {
        nx_time_us_t now;
        nx_result_t result = clock_read(owner, &now);
        if (result != NX_SUCCESS || now >= deadline)
            return result != NX_SUCCESS ? result : NX_ERROR_TIMEOUT;
        switch (owner->phase) {
            case CONFIGURED:
                result =
                    command(owner,
                            owner->resume_selfref ? FMC_SDRAM_CMD_NORMAL_MODE
                                                  : FMC_SDRAM_CMD_CLK_ENABLE,
                            CLOCK_PENDING, deadline);
                break;
            case CLOCK_PENDING: {
                result = wait_ready(owner, deadline);
                if (result != NX_SUCCESS)
                    break;
                /* Anchor after actual completion, including a delayed write. */
                result = clock_read(owner, &now);
                if (result != NX_SUCCESS || now >= deadline)
                    return result != NX_SUCCESS ? result : NX_ERROR_TIMEOUT;
                uint64_t delay = owner->config->powerup_us;
                if (owner->resume_selfref) {
                    FMC_SDRAM_TimingTypeDef timing;
                    uint32_t refresh;
                    if (!plan(owner->config, &timing, &refresh))
                        return NX_ERROR_STATE;
                    uint64_t exit_us =
                        ((uint64_t)timing.ExitSelfRefreshDelay *
                             owner->config->sdclk_div * UINT64_C(1000000) +
                         owner->config->hclk_hz - 1U) /
                        owner->config->hclk_hz;
                    if (delay < exit_us)
                        delay = exit_us;
                }
                if (delay + 1U >= NX_DEADLINE_NEVER - now)
                    return NX_ERROR_STATE;
                owner->powerup_until = now + delay + 1U;
                owner->resume_selfref = false;
                owner->phase = POWERUP_DELAY;
                break;
            }
            case POWERUP_DELAY:
                if (now < owner->powerup_until)
                    continue;
                result =
                    command(owner, FMC_SDRAM_CMD_PALL, PALL_PENDING, deadline);
                break;
            case PALL_PENDING:
                result = wait_ready(owner, deadline);
                if (result == NX_SUCCESS)
                    result = command(owner, FMC_SDRAM_CMD_AUTOREFRESH_MODE,
                                     REFRESH_PENDING, deadline);
                break;
            case REFRESH_PENDING:
                result = wait_ready(owner, deadline);
                if (result == NX_SUCCESS)
                    result = command(owner, FMC_SDRAM_CMD_LOAD_MODE,
                                     MODE_PENDING, deadline);
                break;
            case MODE_PENDING:
                result = wait_ready(owner, deadline);
                if (result == NX_SUCCESS)
                    owner->phase = REFRESH_WRITE;
                break;
            case REFRESH_WRITE: {
                FMC_SDRAM_TimingTypeDef timing;
                uint32_t refresh;
                if (!plan(owner->config, &timing, &refresh))
                    return NX_ERROR_STATE;
                owner->phase = READY; /* Never reprogram a potentially live
                                         refresh timer. */
                (void)FMC_SDRAM_ProgramRefreshRate(FMC_Bank5_6, refresh);
                nx_arch_dsb();
                result = (READ_REG(FMC_Bank5_6->SDRTR) & FMC_SDRTR_COUNT) ==
                                 (refresh << FMC_SDRTR_COUNT_Pos)
                             ? NX_SUCCESS
                             : NX_ERROR_IO;
                break;
            }
            default:
                return NX_ERROR_STATE;
        }
        if (result != NX_SUCCESS)
            return result;
    }
    return NX_SUCCESS;
}

static nx_result_t admission(const nx_stm32_sdram_t* owner,
                             const nx_stm32_sdram_config_t* config,
                             FMC_SDRAM_TimingTypeDef* timing,
                             uint32_t* refresh) {
    if (!context())
        return NX_ERROR_CONTEXT;
    if (!owner_valid(owner) ||
        !span(config, sizeof(*config), _Alignof(nx_stm32_sdram_config_t),
              false) ||
        overlap(owner, sizeof(*owner), config, sizeof(*config)) ||
        config->system != &g_nx_stm32_system ||
        overlap(owner, sizeof(*owner), config->system,
                sizeof(*config->system)) ||
        overlap(config, sizeof(*config), config->system,
                sizeof(*config->system)))
        return NX_ERROR_INVALID;
    if (owner->config != NULL || owner->lease != NULL || owner->gate_added ||
        owner->captured || owner->prepared || owner->phase != 0U ||
        owner->system_epoch != 0U)
        return NX_ERROR_BUSY;
    if (!plan(config, timing, refresh))
        return NX_ERROR_INVALID;
#if !defined(NX_STM32_SDRAM_GRAPHICS)
    if (config->graphics_only)
        return NX_ERROR_UNSUPPORTED;
#endif
    if (config->hclk_hz != NEXUS_CORE_HZ)
        return NX_ERROR_UNSUPPORTED;
    nx_result_t result = pins_valid(owner, config, false);
    if (result != NX_SUCCESS)
        return result;
    return (READ_REG(RCC->AHB3ENR) & RCC_AHB3ENR_FMCEN) != 0U ? NX_ERROR_BUSY
                                                              : NX_SUCCESS;
}
nx_result_t nx_stm32_sdram_admit(const nx_stm32_sdram_t* owner,
                                 const nx_stm32_sdram_config_t* config) {
    FMC_SDRAM_TimingTypeDef timing;
    uint32_t refresh;
    return admission(owner, config, &timing, &refresh);
}
nx_result_t nx_stm32_sdram_start(nx_stm32_sdram_t* owner,
                                 const nx_stm32_sdram_config_t* config) {
    nx_result_t result = nx_stm32_sdram_admit(owner, config);
    if (result != NX_SUCCESS)
        return result;
    if (!system_live(config))
        return NX_ERROR_STATE;
    uint64_t epoch = config->system->time_epoch;
    nx_time_us_t now = nx_stm32_system_clock_read(config->system);
    if (!system_live(config) || epoch != config->system->time_epoch)
        return NX_ERROR_STATE;
    nx_time_us_t deadline;
    result =
        nx_deadline_finite_after(now, config->prepare_budget_us, &deadline);
    return result == NX_SUCCESS
               ? nx_stm32_sdram_prepare(owner, config, deadline)
               : result;
}

nx_result_t nx_stm32_sdram_prepare(nx_stm32_sdram_t* owner,
                                   const nx_stm32_sdram_config_t* config,
                                   nx_time_us_t deadline) {
    FMC_SDRAM_TimingTypeDef timing;
    uint32_t refresh;
    nx_result_t result = admission(owner, config, &timing, &refresh);
    if (result != NX_SUCCESS)
        return result;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (!system_live(config))
        return NX_ERROR_STATE;
    if (!clock_matches(config))
        return NX_ERROR_UNSUPPORTED;
    if ((READ_REG(RCC->APB2ENR) & RCC_APB2ENR_SYSCFGEN) == 0U)
        return NX_ERROR_STATE;
    if (!mapping_matches())
        return NX_ERROR_UNSUPPORTED;
    result = pins_valid(owner, config, true);
    if (result != NX_SUCCESS)
        return result;
    uint64_t system_epoch = config->system->time_epoch;
    nx_time_us_t now = nx_stm32_system_clock_read(config->system);
    if (!system_live(config) || config->system->time_epoch != system_epoch ||
        now == NX_DEADLINE_NEVER)
        return NX_ERROR_STATE;
    if (now >= deadline)
        return NX_ERROR_TIMEOUT;
    if ((READ_REG(RCC->AHB3ENR) & RCC_AHB3ENR_FMCEN) != 0U)
        return NX_ERROR_BUSY;
    owner->config = config;
    owner->system_epoch = system_epoch;
    owner->last_clock = now;
    owner->gate_added = true;
    SET_BIT(RCC->AHB3ENR, RCC_AHB3ENR_FMCEN);
    nx_arch_dsb();
    if ((READ_REG(RCC->AHB3ENR) & RCC_AHB3ENR_FMCEN) == 0U)
        return NX_ERROR_IO;
    owner->saved_sdcr = READ_REG(FMC_Bank5_6->SDCR[0]);
    owner->saved_sdtr = READ_REG(FMC_Bank5_6->SDTR[0]);
    owner->saved_unused_wr = READ_REG(FMC_Bank5_6->SDTR[1]) & FMC_SDTR2_TWR;
    owner->saved_sdrtr = READ_REG(FMC_Bank5_6->SDRTR);
    owner->captured = true;
    if (!write_fifo_enabled())
        return NX_ERROR_UNSUPPORTED;
    if ((owner->saved_sdcr & FMC_SDCR1_SDCLK) != 0U ||
        owner->saved_sdrtr != 0U || READ_REG(FMC_Bank5_6->SDCR[1]) != 0x2d0U ||
        READ_REG(FMC_Bank5_6->SDSR) !=
            (owner->resume_selfref ? FMC_SDSR_MODES1_0 : 0U))
        return NX_ERROR_BUSY;
    result = configure_pins(config);
    if (result != NX_SUCCESS)
        return result;
    FMC_SDRAM_InitTypeDef selected = {
        FMC_SDRAM_BANK1,
        FMC_SDRAM_COLUMN_BITS_NUM_8,
        FMC_SDRAM_ROW_BITS_NUM_12,
        FMC_SDRAM_MEM_BUS_WIDTH_16,
        FMC_SDRAM_INTERN_BANKS_NUM_4,
        (uint32_t)config->cas << FMC_SDCR1_CAS_Pos,
        FMC_SDRAM_WRITE_PROTECTION_DISABLE,
        (uint32_t)config->sdclk_div << FMC_SDCR1_SDCLK_Pos,
        FMC_SDRAM_RBURST_DISABLE,
        (uint32_t)config->read_pipe << FMC_SDCR1_RPIPE_Pos};
    (void)FMC_SDRAM_Init(FMC_Bank5_6, &selected);
    (void)FMC_SDRAM_Timing_Init(FMC_Bank5_6, &timing, FMC_SDRAM_BANK1);
    MODIFY_REG(FMC_Bank5_6->SDTR[1], FMC_SDTR2_TWR, FMC_SDTR2_TWR);
    nx_arch_dsb();
    uint32_t expected = selected.ColumnBitsNumber | selected.RowBitsNumber |
                        selected.MemoryDataWidth | selected.InternalBankNumber |
                        selected.CASLatency | selected.WriteProtection |
                        selected.SDClockPeriod | selected.ReadBurst |
                        selected.ReadPipeDelay;
    uint32_t expected_timing =
        (timing.LoadToActiveDelay - 1U) |
        (timing.ExitSelfRefreshDelay - 1U) << FMC_SDTR1_TXSR_Pos |
        (timing.SelfRefreshTime - 1U) << FMC_SDTR1_TRAS_Pos |
        (timing.RowCycleDelay - 1U) << FMC_SDTR1_TRC_Pos |
        (timing.WriteRecoveryTime - 1U) << FMC_SDTR1_TWR_Pos |
        (timing.RPDelay - 1U) << FMC_SDTR1_TRP_Pos |
        (timing.RCDDelay - 1U) << FMC_SDTR1_TRCD_Pos;
    if (READ_REG(FMC_Bank5_6->SDCR[0]) != expected ||
        READ_REG(FMC_Bank5_6->SDTR[0]) != expected_timing ||
        (READ_REG(FMC_Bank5_6->SDTR[1]) & FMC_SDTR2_TWR) != FMC_SDTR2_TWR)
        return NX_ERROR_IO;
    owner->phase = CONFIGURED;
    result = initialize(owner, deadline);
    if (result == NX_SUCCESS)
        owner->prepared = true;
    return result;
}

nx_result_t nx_stm32_sdram_stop(nx_stm32_sdram_t* owner) {
    if (!context())
        return NX_ERROR_CONTEXT;
    if (!owner_valid(owner))
        return NX_ERROR_INVALID;
    if (owner->config == NULL)
        return NX_SUCCESS;
    owner->prepared = false;
    if (owner->lease != NULL)
        return NX_ERROR_BUSY;
    nx_time_us_t now;
    nx_result_t result = clock_read(owner, &now);
    if (result != NX_SUCCESS ||
        owner->config->stop_budget_us >= NX_DEADLINE_NEVER - now)
        return result != NX_SUCCESS ? result : NX_ERROR_STATE;
    nx_time_us_t deadline = now + owner->config->stop_budget_us;
    if (!clock_matches(owner->config))
        return NX_ERROR_UNSUPPORTED;
    if (owner->captured) {
        if ((READ_REG(RCC->AHB3ENR) & RCC_AHB3ENR_FMCEN) == 0U)
            return NX_ERROR_IO;
        if (owner->phase != 0U && !write_fifo_enabled())
            return NX_ERROR_UNSUPPORTED;
        if ((owner->phase == 0U || owner->phase == QUIESCENT) &&
            (READ_REG(FMC_Bank5_6->SDSR) & FMC_SDSR_BUSY) != 0U)
            return NX_ERROR_BUSY;
        if (owner->phase != 0U && owner->phase < READY) {
            result = initialize(owner, deadline);
            if (result != NX_SUCCESS)
                return result;
        }
        if (owner->phase == READY) {
            result = command(owner, FMC_SDRAM_CMD_SELFREFRESH_MODE,
                             SELF_PENDING, deadline);
            if (result != NX_SUCCESS)
                return result;
        }
        if (owner->phase == SELF_PENDING) {
            result = wait_ready(owner, deadline);
            if (result != NX_SUCCESS)
                return result;
            if ((READ_REG(FMC_Bank5_6->SDSR) & FMC_SDSR_MODES1) !=
                FMC_SDSR_MODES1_0)
                return NX_ERROR_IO;
            owner->resume_selfref = true;
            owner->phase = QUIESCENT;
        }
        WRITE_REG(FMC_Bank5_6->SDCR[0], owner->saved_sdcr);
        WRITE_REG(FMC_Bank5_6->SDTR[0], owner->saved_sdtr);
        MODIFY_REG(FMC_Bank5_6->SDTR[1], FMC_SDTR2_TWR, owner->saved_unused_wr);
        WRITE_REG(FMC_Bank5_6->SDRTR, owner->saved_sdrtr);
        nx_arch_dsb();
        if (READ_REG(FMC_Bank5_6->SDCR[0]) != owner->saved_sdcr ||
            READ_REG(FMC_Bank5_6->SDTR[0]) != owner->saved_sdtr ||
            (READ_REG(FMC_Bank5_6->SDTR[1]) & FMC_SDTR2_TWR) !=
                owner->saved_unused_wr ||
            READ_REG(FMC_Bank5_6->SDRTR) != owner->saved_sdrtr)
            return NX_ERROR_IO;
    }
    CLEAR_BIT(RCC->AHB3ENR, RCC_AHB3ENR_FMCEN);
    nx_arch_dsb();
    if ((READ_REG(RCC->AHB3ENR) & RCC_AHB3ENR_FMCEN) != 0U)
        return NX_ERROR_IO;
    owner->config = NULL;
    owner->system_epoch = 0U;
    owner->gate_added = false;
    owner->captured = false;
    owner->phase = 0U;
    owner->last_clock = 0U;
    owner->powerup_until = 0U;
    return NX_SUCCESS;
}

static bool output_storage(const nx_stm32_sdram_t* owner, const void* output,
                           size_t bytes, size_t align, bool original_lease) {
    const nx_stm32_sdram_config_t* config = owner->config;
    if (!span(output, bytes, align, true) ||
        overlap(output, bytes, owner, sizeof(*owner)) ||
        overlap(output, bytes, config, sizeof(*config)) ||
        overlap(output, bytes, config->pins,
                config->pin_count * sizeof(*config->pins)) ||
        overlap(output, bytes, config->system, sizeof(*config->system)) ||
        (!original_lease && owner->lease != NULL &&
         overlap(output, bytes, owner->lease, sizeof(*owner->lease))))
        return false;
    for (size_t i = 0U; i < config->pin_count; ++i)
        if (overlap(output, bytes, config->pins[i].snapshot, sizeof(uint16_t)))
            return false;
    return true;
}
static nx_external_bus_state_t state(const void* selected) {
    const nx_stm32_sdram_t* owner = selected;
    if (!owner_valid(owner))
        return NX_EXTERNAL_BUS_UNAVAILABLE;
    return owner->config == NULL ? NX_EXTERNAL_BUS_STOPPED
           : owner->prepared && epoch_live(owner)
               ? NX_EXTERNAL_BUS_PREPARED
               : NX_EXTERNAL_BUS_CLEANUP_PENDING;
}
static nx_result_t stop(void* selected) {
    return nx_stm32_sdram_stop(selected);
}
static nx_result_t acquire_window(void* selected,
                                  nx_memory_window_lease_t* lease) {
    nx_stm32_sdram_t* owner = selected;
    if (!context())
        return NX_ERROR_CONTEXT;
    if (!owner_valid(owner) || owner->config == NULL || !owner->prepared ||
        !epoch_live(owner))
        return NX_ERROR_STATE;
    if (!output_storage(owner, lease, sizeof(*lease),
                        _Alignof(nx_memory_window_lease_t), false))
        return NX_ERROR_INVALID;
    if (owner->lease != NULL)
        return NX_ERROR_BUSY;
    if (lease->owner != NULL || lease->address != 0U || lease->bytes != 0U ||
        lease->epoch != 0U || owner->epoch == UINT64_MAX)
        return NX_ERROR_STATE;
    if (!write_fifo_enabled())
        return NX_ERROR_UNSUPPORTED;
    if (!clock_matches(owner->config) || !mapping_matches() ||
        (READ_REG(FMC_Bank5_6->SDSR) &
         (FMC_SDSR_BUSY | FMC_SDSR_RE | FMC_SDSR_MODES1)) != 0U)
        return NX_ERROR_IO;
    ++owner->epoch;
    owner->lease = lease;
    *lease = (nx_memory_window_lease_t){owner, UINT32_C(0xc0000000),
                                        owner->config->bytes, owner->epoch};
    return NX_SUCCESS;
}
static nx_result_t release_window(void* selected,
                                  nx_memory_window_lease_t* lease,
                                  uint64_t epoch) {
    nx_stm32_sdram_t* owner = selected;
    if (!context())
        return NX_ERROR_CONTEXT;
    if (!owner_valid(owner) || owner->config == NULL || owner->lease != lease ||
        !output_storage(owner, lease, sizeof(*lease),
                        _Alignof(nx_memory_window_lease_t), true) ||
        lease->owner != owner || lease->epoch != epoch ||
        epoch != owner->epoch || lease->address != UINT32_C(0xc0000000) ||
        lease->bytes != owner->config->bytes)
        return NX_ERROR_INVALID;
    nx_arch_dsb();
    owner->lease = NULL;
    *lease = (nx_memory_window_lease_t){0};
    return NX_SUCCESS;
}
/** \brief A graphics-owned bank never publishes its private hold as CPU data.
 */
static nx_result_t acquire(void* selected, nx_memory_window_lease_t* lease) {
    nx_stm32_sdram_t* owner = selected;
    if (!context())
        return NX_ERROR_CONTEXT;
    if (owner_valid(owner) && owner->config != NULL &&
        owner->config->graphics_only)
        return NX_ERROR_UNSUPPORTED;
    return acquire_window(selected, lease);
}
/** \brief Public release cannot detach a selected graphics owner's hold. */
static nx_result_t release(void* selected, nx_memory_window_lease_t* lease,
                           uint64_t epoch) {
    nx_stm32_sdram_t* owner = selected;
    if (!context())
        return NX_ERROR_CONTEXT;
    if (owner_valid(owner) && owner->config != NULL &&
        owner->config->graphics_only)
        return NX_ERROR_UNSUPPORTED;
    return release_window(selected, lease, epoch);
}
#if defined(NX_STM32_SDRAM_GRAPHICS)
bool nx_stm32_sdram_graphics_overlaps(const nx_stm32_sdram_graphics_t* binding,
                                      const void* storage, size_t bytes) {
    if (!span(binding, sizeof(*binding), _Alignof(nx_stm32_sdram_graphics_t),
              true) ||
        !owner_valid(binding->owner) || storage == NULL || bytes == 0U ||
        bytes > UINTPTR_MAX - (uintptr_t)storage)
        return true;
    const nx_stm32_sdram_t* owner = binding->owner;
    const nx_stm32_sdram_config_t* config = owner->config;
    if (config == NULL || overlap(storage, bytes, binding, sizeof(*binding)) ||
        overlap(storage, bytes, owner, sizeof(*owner)) ||
        overlap(storage, bytes, config, sizeof(*config)) ||
        overlap(storage, bytes, config->pins,
                config->pin_count * sizeof(*config->pins)) ||
        overlap(storage, bytes, config->system, sizeof(*config->system)) ||
        (owner->lease != NULL &&
         overlap(storage, bytes, owner->lease, sizeof(*owner->lease))))
        return true;
    for (size_t i = 0U; i < config->pin_count; ++i)
        if (overlap(storage, bytes, config->pins[i].snapshot, sizeof(uint16_t)))
            return true;
    return false;
}
/** \brief Validate complete stable binding storage before using its owner. */
static nx_result_t graphics_binding(const nx_stm32_sdram_graphics_t* binding,
                                    unsigned master, const void* consumer,
                                    bool admission) {
    if (!context())
        return NX_ERROR_CONTEXT;
    if (!span(binding, sizeof(*binding), _Alignof(nx_stm32_sdram_graphics_t),
              true) ||
        master >= NX_STM32_SDRAM_GRAPHICS_MASTER_COUNT || consumer == NULL ||
        !owner_valid(binding->owner))
        return NX_ERROR_INVALID;
    const nx_stm32_sdram_t* owner = binding->owner;
    if (owner->config == NULL || (admission && !owner->prepared))
        return NX_ERROR_STATE;
    if (!output_storage(owner, binding, sizeof(*binding),
                        _Alignof(nx_stm32_sdram_graphics_t), true))
        return NX_ERROR_INVALID;
    if (!owner->config->graphics_only)
        return NX_ERROR_UNSUPPORTED;
    return epoch_live(owner) ? NX_SUCCESS : NX_ERROR_STATE;
}
nx_result_t
nx_stm32_sdram_graphics_check(const nx_stm32_sdram_graphics_t* binding,
                              nx_stm32_sdram_graphics_master_t master,
                              const void* consumer, const void* system) {
    nx_result_t result =
        graphics_binding(binding, (unsigned)master, consumer, true);
    if (result != NX_SUCCESS)
        return result;
    const nx_stm32_sdram_t* owner = binding->owner;
    if (system != owner->config->system)
        return NX_ERROR_INVALID;
    if (binding->consumers[master] != consumer ||
        owner->lease != &binding->lease || binding->lease.owner != owner ||
        binding->lease.epoch != owner->epoch ||
        binding->lease.address != UINT32_C(0xc0000000) ||
        binding->lease.bytes != owner->config->bytes)
        return NX_ERROR_STATE;
    if (!write_fifo_enabled() || !clock_matches(owner->config) ||
        !mapping_matches() ||
        (READ_REG(FMC_Bank5_6->SDSR) &
         (FMC_SDSR_BUSY | FMC_SDSR_RE | FMC_SDSR_MODES1)) != 0U)
        return NX_ERROR_IO;
    return NX_SUCCESS;
}
nx_result_t
nx_stm32_sdram_graphics_attach(nx_stm32_sdram_graphics_t* binding,
                               nx_stm32_sdram_graphics_master_t master,
                               const void* consumer, const void* system) {
    nx_result_t result =
        graphics_binding(binding, (unsigned)master, consumer, true);
    if (result != NX_SUCCESS)
        return result;
    nx_stm32_sdram_t* owner = binding->owner;
    if (system != owner->config->system)
        return NX_ERROR_INVALID;
    if (binding->consumers[master] != NULL ||
        binding->consumers[1U - (unsigned)master] == consumer)
        return NX_ERROR_BUSY;
    if (owner->lease == NULL) {
        if (binding->consumers[0] != NULL || binding->consumers[1] != NULL)
            return NX_ERROR_STATE;
        result = acquire_window(owner, &binding->lease);
        if (result != NX_SUCCESS)
            return result;
    } else if (owner->lease != &binding->lease ||
               binding->lease.owner != owner ||
               binding->lease.epoch != owner->epoch) {
        return NX_ERROR_BUSY;
    } else {
        unsigned peer = 1U - (unsigned)master;
        if (binding->consumers[peer] == NULL)
            return NX_ERROR_STATE;
        result = nx_stm32_sdram_graphics_check(
            binding, (nx_stm32_sdram_graphics_master_t)peer,
            binding->consumers[peer], system);
        if (result != NX_SUCCESS)
            return result;
    }
    binding->consumers[master] = consumer;
    return NX_SUCCESS;
}
nx_result_t
nx_stm32_sdram_graphics_detach(nx_stm32_sdram_graphics_t* binding,
                               nx_stm32_sdram_graphics_master_t master,
                               const void* consumer) {
    nx_result_t result =
        graphics_binding(binding, (unsigned)master, consumer, false);
    if (result != NX_SUCCESS)
        return result;
    nx_stm32_sdram_t* owner = binding->owner;
    if (binding->consumers[master] != consumer ||
        owner->lease != &binding->lease || binding->lease.owner != owner ||
        binding->lease.epoch != owner->epoch)
        return NX_ERROR_INVALID;
    if (binding->consumers[1U - (unsigned)master] == NULL) {
        result = release_window(owner, &binding->lease, binding->lease.epoch);
        if (result != NX_SUCCESS)
            return result;
    }
    binding->consumers[master] = NULL;
    return NX_SUCCESS;
}
#endif
static nx_result_t geometry(const void* selected,
                            nx_memory_window_geometry_t* result) {
    const nx_stm32_sdram_t* owner = selected;
    if (!owner_valid(owner) || owner->config == NULL || !owner->prepared ||
        !epoch_live(owner))
        return NX_ERROR_STATE;
    if (!output_storage(owner, result, sizeof(*result),
                        _Alignof(nx_memory_window_geometry_t), false))
        return NX_ERROR_INVALID;
    *result = (nx_memory_window_geometry_t){UINT32_C(0xc0000000),
                                            owner->config->bytes, false};
    return NX_SUCCESS;
}
const nx_external_bus_ops_t nx_stm32_sdram_bus_ops = {state, stop};
const nx_memory_window_ops_t nx_stm32_sdram_window_ops = {acquire, release,
                                                          geometry};
