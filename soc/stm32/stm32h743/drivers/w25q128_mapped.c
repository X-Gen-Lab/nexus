/**
 * \file            w25q128_mapped.c
 * \brief           Bounded H743 single-line W25Q128JV mapped-storage backend
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/core/fault.h"
#include "stm32h743_w25q128.h"
#include <stddef.h>

_Static_assert(offsetof(nx_h743_w25q128_state_t, hal) == 0U,
               "HAL callbacks require the instance handle at offset zero");

#ifndef NX_H743_W25Q128_REGS
#define NX_H743_W25Q128_REGS        QUADSPI
#define NX_H743_W25Q128_RCC         RCC
#define NX_H743_W25Q128_CORE        SCB
#define NX_H743_W25Q128_APERTURE    ((const uint8_t*)QSPI_BASE)
#define NX_H743_W25Q128_READ(p)     (*(p))
#define NX_H743_W25Q128_WRITE(p, v) (*(p) = (v))
#define NX_H743_W25Q128_IRQ_IDLE()                                             \
    (NVIC_GetEnableIRQ(QUADSPI_IRQn) == 0U &&                                  \
     NVIC_GetActive(QUADSPI_IRQn) == 0U)
#define NX_H743_W25Q128_IRQ_PENDING()   (NVIC_GetPendingIRQ(QUADSPI_IRQn) != 0U)
#define NX_H743_W25Q128_CLEAR_PENDING() NVIC_ClearPendingIRQ(QUADSPI_IRQn)
#endif

#ifndef NX_H743_W25Q128_MPU
#define NX_H743_W25Q128_MPU MPU
#endif

#define IRQ_SOURCES                                                            \
    (QSPI_IT_TO | QSPI_IT_SM | QSPI_IT_FT | QSPI_IT_TC | QSPI_IT_TE)
#define STATUS_FLAGS  (QSPI_FLAG_TO | QSPI_FLAG_SM | QSPI_FLAG_TC | QSPI_FLAG_TE)
#define MAP_MODE      ((uint32_t)QUADSPI_CCR_FMODE)
#define ARRAY_BUSY    1U
#define WRITE_ENABLE  2U
#define BLOCK_PROTECT 0x1CU
enum phase {
    POWER_GUARD,
    ID_REPLY,
    CHECK_START,
    STATUS1_REPLY,
    STATUS2_REPLY,
    STATUS3_REPLY,
    SELECT_CHUNK,
    ERASED_REPLY,
    WREN_REPLY,
    WEL_REPLY,
    PULSE_REPLY,
    WIP_REPLY,
    VERIFY_REPLY,
    CLEAN_START,
    CLEAN_REPLY,
    WRDI_REPLY,
    DONE,
    IDLE,
    FAILED
};
/** \brief Complete spans precede arithmetic and known-region comparisons. */
static bool span(const void* data, size_t bytes) {
    return data != NULL && bytes != 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)data;
}
/** \brief Compare two admitted complete spans. */
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
/** \brief Construction and service share one privileged unmasked task owner. */
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief Clock/publisher/cache facts remain true for the complete owner. */
static bool accessible(void) {
    return (NX_H743_W25Q128_READ(&NX_H743_W25Q128_RCC->AHB3ENR) &
            RCC_AHB3ENR_QSPIEN) != 0U &&
           (NX_H743_W25Q128_READ(&NX_H743_W25Q128_CORE->CCR) &
            (SCB_CCR_IC_Msk | SCB_CCR_DC_Msk)) == 0U &&
           NX_H743_W25Q128_IRQ_IDLE();
}
/** \brief A HAL state value never substitutes for these hardware facts. */
static bool idle(void) {
    return (NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->SR) &
            (QUADSPI_SR_BUSY | QUADSPI_SR_FLEVEL)) == 0U &&
           (NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->CR) &
            (QUADSPI_CR_ABORT | QUADSPI_CR_DMAEN)) == 0U;
}
/** \brief Each mutable controller write has an independent readback. */
static bool checked(volatile uint32_t* reg, uint32_t value) {
    NX_H743_W25Q128_WRITE(reg, value);
    nx_arch_dsb();
    return NX_H743_W25Q128_READ(reg) == value;
}

#define MPU_SELECTOR_EFFECT 1U
#define MPU_DENY_EFFECT     2U
#define MPU_READ_EFFECT     4U
#define MPU_APERTURE_BASE   0x90000000U
#define MPU_DENY_ATTRIBUTES                                                    \
    ((uint32_t)MPU_RASR_XN_Msk | (27U << MPU_RASR_SIZE_Pos) |                  \
     (uint32_t)MPU_RASR_ENABLE_Msk)
#define MPU_READ_ATTRIBUTES                                                    \
    ((uint32_t)MPU_RASR_XN_Msk | (6U << MPU_RASR_AP_Pos) |                     \
     (1U << MPU_RASR_TEX_Pos) | (23U << MPU_RASR_SIZE_Pos) |                   \
     (uint32_t)MPU_RASR_ENABLE_Msk)
enum guard_view { GUARD_COLD, GUARD_DENIED, GUARD_READABLE, GUARD_WITHDRAW };

/** \brief Serialize the shared MPU selector without changing global policy. */
static bool mpu_begin(nx_h743_w25q128_state_t* state,
                      nx_arch_irq_state_t* saved) {
    *saved = nx_arch_irq_save();
    if ((state->mpu_effects & MPU_SELECTOR_EFFECT) != 0U) {
        if (!checked(&NX_H743_W25Q128_MPU->RNR, state->mpu_work_selector)) {
            nx_arch_irq_restore(*saved);
            return false;
        }
        state->mpu_effects &= (uint8_t)~MPU_SELECTOR_EFFECT;
    }
    state->mpu_work_selector = NX_H743_W25Q128_READ(&NX_H743_W25Q128_MPU->RNR);
    state->mpu_effects |= MPU_SELECTOR_EFFECT;
    return true;
}
/** \brief Keep failed selector restoration owned and retryable. */
static bool mpu_end(nx_h743_w25q128_state_t* state, nx_arch_irq_state_t saved,
                    bool result) {
    if (checked(&NX_H743_W25Q128_MPU->RNR, state->mpu_work_selector))
        state->mpu_effects &= (uint8_t)~MPU_SELECTOR_EFFECT;
    else
        result = false;
    nx_arch_dsb();
    nx_arch_isb();
    nx_arch_irq_restore(saved);
    return result;
}
/** \brief Conservatively include subregions when rejecting foreign overrides.
 */
static bool mpu_overlap(uint32_t base, uint32_t attributes) {
    if ((attributes & MPU_RASR_ENABLE_Msk) == 0U)
        return false;
    uint32_t exponent =
        (attributes & (uint32_t)MPU_RASR_SIZE_Msk) >> MPU_RASR_SIZE_Pos;
    if (exponent < 4U)
        return true;
    uint64_t size = UINT64_C(1) << (exponent + 1U);
    uint64_t start = ((uint64_t)base & MPU_RBAR_ADDR_Msk) & ~(size - 1U);
    return start < UINT64_C(0xA0000000) && UINT64_C(0x90000000) < start + size;
}
/** \brief Read actual banked descriptors; RBAR low REGION bits are not BASE. */
static bool mpu_policy(nx_h743_w25q128_state_t* state, enum guard_view view) {
    nx_arch_irq_state_t saved;
    if (!mpu_begin(state, &saved))
        return false;
    bool valid =
        NX_H743_W25Q128_READ(&NX_H743_W25Q128_MPU->CTRL) == state->mpu_control;
    for (uint32_t index = 0U; valid && index < 16U; ++index) {
        if (!checked(&NX_H743_W25Q128_MPU->RNR, index)) {
            valid = false;
            break;
        }
        uint32_t base = NX_H743_W25Q128_READ(&NX_H743_W25Q128_MPU->RBAR) &
                        (uint32_t)MPU_RBAR_ADDR_Msk;
        uint32_t attributes = NX_H743_W25Q128_READ(&NX_H743_W25Q128_MPU->RASR);
        if (index == state->mpu_deny_region ||
            index == state->mpu_read_region) {
            unsigned slot = index == state->mpu_deny_region ? 0U : 1U;
            if (view == GUARD_COLD) {
                state->mpu_base[slot] = base;
                state->mpu_attributes[slot] = attributes;
                valid = (attributes & MPU_RASR_ENABLE_Msk) == 0U;
            } else if (slot == 0U) {
                valid = base == MPU_APERTURE_BASE &&
                        attributes == MPU_DENY_ATTRIBUTES;
            } else if (view != GUARD_WITHDRAW) {
                valid = base == MPU_APERTURE_BASE &&
                        attributes ==
                            (view == GUARD_READABLE ? MPU_READ_ATTRIBUTES : 0U);
            }
        } else if (index > state->mpu_deny_region &&
                   mpu_overlap(base, attributes)) {
            valid = false;
        }
    }
    return mpu_end(state, saved, valid);
}
/** \brief A selected bank and masked BASE readback precede permission writes.
 */
static bool mpu_region(nx_h743_w25q128_state_t* state, unsigned slot,
                       uint32_t base, uint32_t attributes) {
    nx_arch_irq_state_t saved;
    if (!mpu_begin(state, &saved))
        return false;
    uint32_t region =
        slot == 0U ? state->mpu_deny_region : state->mpu_read_region;
    bool valid = NX_H743_W25Q128_READ(&NX_H743_W25Q128_MPU->CTRL) ==
                     state->mpu_control &&
                 checked(&NX_H743_W25Q128_MPU->RNR, region);
    /* A live no-access descriptor must never move over executing Flash/RAM.
     * Disable and fence the selected bank before changing its effective BASE.
     */
    if (valid)
        valid = checked(&NX_H743_W25Q128_MPU->RASR, 0U);
    if (valid) {
        nx_arch_isb();
        NX_H743_W25Q128_WRITE(&NX_H743_W25Q128_MPU->RBAR,
                              base & (uint32_t)MPU_RBAR_ADDR_Msk);
        nx_arch_dsb();
        valid = NX_H743_W25Q128_READ(&NX_H743_W25Q128_MPU->RNR) == region &&
                (NX_H743_W25Q128_READ(&NX_H743_W25Q128_MPU->RBAR) &
                 (uint32_t)MPU_RBAR_ADDR_Msk) ==
                    (base & (uint32_t)MPU_RBAR_ADDR_Msk);
    }
    if (valid)
        valid = checked(&NX_H743_W25Q128_MPU->RASR, attributes);
    return mpu_end(state, saved, valid);
}
/** \brief Claim only two disabled slots under an already enabled fault guard.
 */
static nx_result_t mpu_claim(nx_h743_w25q128_state_t* state) {
    if (!mpu_policy(state, GUARD_COLD)) {
        if (state->mpu_effects == 0U)
            state->owned = false;
        return state->mpu_effects == 0U ? NX_ERROR_UNSUPPORTED : NX_ERROR_IO;
    }
    state->mpu_effects |= MPU_DENY_EFFECT;
    if (!mpu_region(state, 0U, MPU_APERTURE_BASE, MPU_DENY_ATTRIBUTES))
        return NX_ERROR_IO;
    state->mpu_effects |= MPU_READ_EFFECT;
    if (!mpu_region(state, 1U, MPU_APERTURE_BASE, 0U) ||
        !mpu_policy(state, GUARD_DENIED))
        return NX_ERROR_IO;
    return NX_SUCCESS;
}
/** \brief Deny first, then fence all outstanding and future CPU accesses. */
static bool mpu_close(nx_h743_w25q128_state_t* state) {
    if (!mpu_policy(state, GUARD_WITHDRAW) ||
        !mpu_region(state, 1U, MPU_APERTURE_BASE, 0U))
        return false;
    state->mpu_readable = false;
    return mpu_policy(state, GUARD_DENIED);
}
/** \brief Grant only the exact read-only, XN, noncacheable16MiB aperture. */
static bool mpu_open(nx_h743_w25q128_state_t* state) {
    if (!mpu_policy(state, GUARD_DENIED) ||
        !mpu_region(state, 1U, MPU_APERTURE_BASE, MPU_READ_ATTRIBUTES))
        return false;
    state->mpu_readable = true;
    return mpu_policy(state, GUARD_READABLE);
}
/** \brief Restore disabled cold descriptors only after the controller is off.
 */
static bool mpu_restore(nx_h743_w25q128_state_t* state) {
    if ((state->mpu_effects & MPU_READ_EFFECT) != 0U) {
        if (!mpu_region(state, 1U, state->mpu_base[1],
                        state->mpu_attributes[1]))
            return false;
        state->mpu_effects &= (uint8_t)~MPU_READ_EFFECT;
    }
    if ((state->mpu_effects & MPU_DENY_EFFECT) != 0U) {
        if (!mpu_region(state, 0U, state->mpu_base[0],
                        state->mpu_attributes[0]))
            return false;
        state->mpu_effects &= (uint8_t)~MPU_DENY_EFFECT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    state->mpu_work_selector = state->mpu_selector;
    state->mpu_effects |= MPU_SELECTOR_EFFECT;
    bool restored = mpu_end(state, saved,
                            NX_H743_W25Q128_READ(&NX_H743_W25Q128_MPU->CTRL) ==
                                state->mpu_control);
    return restored && state->mpu_effects == 0U;
}
/** \brief Callbacks publish facts to the same serialized task executor. */
static void completed(QSPI_HandleTypeDef* hal) {
    ((nx_h743_w25q128_state_t*)hal)->pending = false;
}
/** \brief Error publication never revokes a buffer reference. */
static void error(QSPI_HandleTypeDef* hal) {
    nx_h743_w25q128_state_t* state = (nx_h743_w25q128_state_t*)hal;
    state->transport_error = true;
    state->pending = false;
}
/** \brief Unselected HAL FIFO/status callbacks have no application work. */
static void no_callback(QSPI_HandleTypeDef* hal) {
    (void)hal;
}
/** \brief Only timeout0 HAL wait paths are admitted by this private profile.
 * \note Their first failed predicate exits through the explicit timeout0
 * branch. No positive-timeout HAL operation uses this bridge as a stopped
 * clock.
 */
uint32_t nx_h743_w25q128_hal_get_tick(void) {
    return 0U;
}
/** \brief Latch an accepted terminal fact without releasing storage. */
static void fail(nx_h743_w25q128_state_t* state, nx_result_t result) {
    if (state->result == NX_SUCCESS || state->result == NX_ERROR_BUSY)
        state->result = result;
    state->cancelled = true;
    /* READY/idle can still retain sticky TE and private HAL buffer pointers.
     * Each failure initiates one withdrawal; fresh cleanup commands do not. */
    state->abort_requested = true;
    state->phase = state->active ? CLEAN_START : FAILED;
}
/** \brief Initiate a bounded abort; callbacks alone do not prove completion. */
static nx_result_t abort_step(nx_h743_w25q128_state_t* state) {
    if (!accessible())
        return NX_ERROR_IO;
    if (!state->aborting) {
        state->aborting = true;
        if (HAL_QSPI_Abort_IT(&state->hal) != HAL_OK)
            return NX_ERROR_IO;
        /* Abort_IT skips ERROR states. Hardware BUSY still needs an abort;
         * this bounded gap uses official masks and the normal HAL completion.
         */
        if ((NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->SR) &
             QUADSPI_SR_BUSY) != 0U &&
            state->hal.State != HAL_QSPI_STATE_ABORT) {
            state->hal.State = HAL_QSPI_STATE_ABORT;
            uint32_t control = NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->CR);
            if (!checked(&NX_H743_W25Q128_REGS->CR,
                         (control & ~(uint32_t)IRQ_SOURCES) | QUADSPI_CR_ABORT |
                             QSPI_IT_TC))
                return NX_ERROR_IO;
        }
        state->pending = state->hal.State == HAL_QSPI_STATE_ABORT;
    }
    if (state->pending)
        HAL_QSPI_IRQHandler(&state->hal);
    return state->pending || !idle() ? NX_ERROR_BUSY : NX_SUCCESS;
}
/** \brief Remove future controller/IRQ/data references before final
 * publication. */
static nx_result_t withdraw(nx_h743_w25q128_state_t* state) {
    if (!accessible())
        return NX_ERROR_IO;
    if (state->pending || !idle())
        return NX_ERROR_BUSY;
    uint32_t control = NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->CR);
    if (!checked(&NX_H743_W25Q128_REGS->CR, control & ~(uint32_t)IRQ_SOURCES) ||
        !checked(&NX_H743_W25Q128_REGS->CCR, 0U) ||
        !checked(&NX_H743_W25Q128_REGS->DLR, 0U) ||
        !checked(&NX_H743_W25Q128_REGS->AR, 0U) ||
        !checked(&NX_H743_W25Q128_REGS->ABR, 0U) ||
        !checked(&NX_H743_W25Q128_REGS->LPTR, 0U))
        return NX_ERROR_IO;
    NX_H743_W25Q128_WRITE(&NX_H743_W25Q128_REGS->FCR, STATUS_FLAGS);
    nx_arch_dsb();
    NX_H743_W25Q128_CLEAR_PENDING();
    nx_arch_dsb();
    if (!idle() || !NX_H743_W25Q128_IRQ_IDLE() ||
        NX_H743_W25Q128_IRQ_PENDING() ||
        (NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->SR) & STATUS_FLAGS) != 0U)
        return NX_ERROR_IO;
    state->hal.pTxBuffPtr = NULL;
    state->hal.pRxBuffPtr = NULL;
    state->hal.TxXferCount = state->hal.TxXferSize = 0U;
    state->hal.RxXferCount = state->hal.RxXferSize = 0U;
    state->hal.ErrorCode = HAL_QSPI_ERROR_NONE;
    state->hal.State = HAL_QSPI_STATE_READY;
    state->transport_error = state->aborting = false;
    return NX_SUCCESS;
}
/** \brief Construct one exact single-line24-bit command. */
static QSPI_CommandTypeDef command(uint8_t opcode, size_t address,
                                   size_t count) {
    QSPI_CommandTypeDef value = {0};
    value.Instruction = opcode;
    value.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    value.Address = (uint32_t)address;
    value.AddressMode = opcode == 0x03U || opcode == 0x02U || opcode == 0x20U
                            ? QSPI_ADDRESS_1_LINE
                            : QSPI_ADDRESS_NONE;
    value.AddressSize = QSPI_ADDRESS_24_BITS;
    value.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    value.DataMode = count != 0U ? QSPI_DATA_1_LINE : QSPI_DATA_NONE;
    value.NbData = (uint32_t)count;
    value.DdrMode = QSPI_DDR_MODE_DISABLE;
    value.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    value.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    return value;
}
/** \brief Admit at most256 TX or16 RX bytes before complete HAL dispatch. */
static bool issue(nx_h743_w25q128_state_t* state, uint8_t opcode,
                  size_t address, size_t count, bool tx, enum phase next) {
    if (!accessible() || !idle() || state->pending ||
        state->hal.State != HAL_QSPI_STATE_READY ||
        count >
            (tx ? NX_H743_W25Q128_PAGE_BYTES : NX_H743_W25Q128_CHECK_BYTES)) {
        fail(state, NX_ERROR_IO);
        return false;
    }
    if ((NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->SR) & QSPI_FLAG_TE) !=
            0U ||
        (NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->CR) & IRQ_SOURCES) != 0U) {
        fail(state, NX_ERROR_IO);
        return false;
    }
    QSPI_CommandTypeDef value = command(opcode, address, count);
    /* Stage and verify an addressed command before an irreversible no-data
     * command can trigger inside the official HAL configuration function. */
    if (value.AddressMode != QSPI_ADDRESS_NONE &&
        (!checked(&NX_H743_W25Q128_REGS->CCR, 0U) ||
         !checked(&NX_H743_W25Q128_REGS->AR, value.Address))) {
        fail(state, NX_ERROR_IO);
        return false;
    }
    HAL_QSPI_SetTimeout(&state->hal, 0U);
    if (HAL_QSPI_Command_IT(&state->hal, &value) != HAL_OK) {
        fail(state, NX_ERROR_IO);
        return false;
    }
    uint32_t mask = QUADSPI_CCR_INSTRUCTION | QUADSPI_CCR_IMODE |
                    QUADSPI_CCR_ADMODE | QUADSPI_CCR_DMODE | QUADSPI_CCR_FMODE;
    uint32_t expected = value.Instruction | value.InstructionMode |
                        value.AddressMode | value.DataMode;
    if (value.AddressMode != QSPI_ADDRESS_NONE) {
        mask |= QUADSPI_CCR_ADSIZE;
        expected |= value.AddressSize;
    }
    if ((NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->CCR) & mask) != expected ||
        (value.AddressMode != QSPI_ADDRESS_NONE &&
         NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->AR) != value.Address) ||
        (count != 0U &&
         NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->DLR) != count - 1U)) {
        fail(state, NX_ERROR_IO);
        return false;
    }
    state->command = opcode;
    state->phase = (uint8_t)next;
    state->pending = true;
    state->deadline =
        nx_deadline_after(state->clock.read(state->clock.context), 2000U);
    HAL_StatusTypeDef status = HAL_OK;
    if (count != 0U)
        status = tx ? HAL_QSPI_Transmit_IT(
                          &state->hal, (uint8_t*)(uintptr_t)(state->input +
                                                             state->completed))
                    : HAL_QSPI_Receive_IT(&state->hal, state->response);
    if (status != HAL_OK) {
        fail(state, NX_ERROR_IO);
        return false;
    }
    return true;
}
/** \brief A status observation is still a finite physical command. */
static void status_read(nx_h743_w25q128_state_t* state, enum phase next) {
    (void)issue(state, 0x05U, 0U, 1U, false, next);
}
/** \brief Read at most16 bytes at one verification boundary. */
static void read_piece(nx_h743_w25q128_state_t* state, enum phase next) {
    size_t count = state->chunk - state->checked;
    if (count > NX_H743_W25Q128_CHECK_BYTES)
        count = NX_H743_W25Q128_CHECK_BYTES;
    (void)issue(state, 0x03U, state->offset + state->completed + state->checked,
                count, false, next);
}
/** \brief Bounded HAL counters and actual idle precede the next command. */
static bool pump(nx_h743_w25q128_state_t* state, nx_time_us_t now) {
    if (state->hal.TxXferCount > NX_H743_W25Q128_PAGE_BYTES ||
        state->hal.RxXferCount > NX_H743_W25Q128_CHECK_BYTES ||
        (NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->CR) & QUADSPI_CR_DMAEN) !=
            0U) {
        fail(state, NX_ERROR_IO);
        return false;
    }
    bool pending = state->pending;
    uint32_t before = NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->SR);
    if (pending && (before & QSPI_FLAG_TE) == 0U)
        HAL_QSPI_IRQHandler(&state->hal);
    uint32_t after = NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->SR);
    if (state->transport_error || ((before | after) & QSPI_FLAG_TE) != 0U ||
        state->hal.ErrorCode != HAL_QSPI_ERROR_NONE) {
        fail(state, NX_ERROR_IO);
        return false;
    }
    if (pending && !state->pending &&
        ((state->command == 0x02U && state->hal.TxXferCount != 0U) ||
         ((state->command == 0x9FU || state->command == 0x05U ||
           state->command == 0x35U || state->command == 0x15U ||
           state->command == 0x03U) &&
          state->hal.RxXferCount != 0U))) {
        fail(state, NX_ERROR_IO);
        return false;
    }
    if (state->pending || !idle()) {
        if (nx_deadline_expired(state->deadline, now))
            fail(state, NX_ERROR_TIMEOUT);
        return false;
    }
    return true;
}
/** \brief Progress one command/verification phase without hidden waiting. */
static void step(nx_h743_w25q128_state_t* state) {
    if (!state->owned || !accessible()) {
        if (state->owned)
            fail(state, NX_ERROR_IO);
        return;
    }
    nx_time_us_t now = state->clock.read(state->clock.context);
    if (state->aborting || state->transport_error || state->abort_requested) {
        if (abort_step(state) != NX_SUCCESS || withdraw(state) != NX_SUCCESS)
            return;
        state->abort_requested = false;
    }
    if (state->mapped)
        return;
    if (!pump(state, now))
        return;
    if (state->phase == FAILED || state->phase == IDLE || state->phase == DONE)
        return;
    if (!state->ready && nx_deadline_expired(state->startup_deadline, now)) {
        fail(state, NX_ERROR_TIMEOUT);
        return;
    }
    if (state->active && state->cancelled && !state->pulse &&
        state->phase != CLEAN_START && state->phase != CLEAN_REPLY &&
        state->phase != WRDI_REPLY)
        state->phase = CLEAN_START;
    switch ((enum phase)state->phase) {
        case POWER_GUARD:
            if (now >= state->power_until)
                (void)issue(state, 0x9FU, 0U, 3U, false, ID_REPLY);
            break;
        case ID_REPLY:
            if (state->response[0] != 0xEFU || state->response[1] != 0x40U ||
                state->response[2] != 0x18U) {
                fail(state, NX_ERROR_UNSUPPORTED);
                break;
            }
            status_read(state, STATUS1_REPLY);
            break;
        case CHECK_START:
            status_read(state, STATUS1_REPLY);
            break;
        case STATUS1_REPLY:
            state->status1 = state->response[0];
            if ((state->status1 & ARRAY_BUSY) != 0U)
                status_read(state, STATUS1_REPLY);
            else if ((state->status1 & BLOCK_PROTECT) != 0U)
                fail(state, NX_ERROR_UNSUPPORTED);
            else
                (void)issue(state, 0x35U, 0U, 1U, false, STATUS2_REPLY);
            break;
        case STATUS2_REPLY:
            state->status2 = state->response[0];
            if ((state->status2 & 0xC0U) != 0U)
                fail(state, NX_ERROR_UNSUPPORTED);
            else
                (void)issue(state, 0x15U, 0U, 1U, false, STATUS3_REPLY);
            break;
        case STATUS3_REPLY:
            state->status3 = state->response[0];
            if ((state->status3 & 0x04U) != 0U)
                fail(state, NX_ERROR_UNSUPPORTED);
            else if (state->active)
                state->phase = SELECT_CHUNK;
            else if ((state->status1 & WRITE_ENABLE) != 0U)
                state->phase = CLEAN_START;
            else {
                state->ready = true;
                state->result = NX_SUCCESS;
                state->phase = IDLE;
            }
            break;
        case SELECT_CHUNK:
            if (state->completed == state->length) {
                state->phase = DONE;
                break;
            }
            state->checked = 0U;
            state->chunk = state->operation == NX_MAPPED_STORAGE_PROGRAM
                               ? NX_H743_W25Q128_PAGE_BYTES -
                                     (state->offset + state->completed) %
                                         NX_H743_W25Q128_PAGE_BYTES
                               : NX_H743_W25Q128_ERASE_BYTES;
            if (state->chunk > state->length - state->completed)
                state->chunk = state->length - state->completed;
            if (state->operation == NX_MAPPED_STORAGE_PROGRAM)
                read_piece(state, ERASED_REPLY);
            else
                (void)issue(state, 0x06U, 0U, 0U, false, WREN_REPLY);
            break;
        case ERASED_REPLY: {
            size_t count = state->chunk - state->checked;
            if (count > NX_H743_W25Q128_CHECK_BYTES)
                count = NX_H743_W25Q128_CHECK_BYTES;
            for (size_t i = 0U; i < count; ++i)
                if (state->response[i] != UINT8_MAX) {
                    fail(state, NX_ERROR_IO);
                    return;
                }
            state->checked += count;
            if (state->checked < state->chunk)
                read_piece(state, ERASED_REPLY);
            else
                (void)issue(state, 0x06U, 0U, 0U, false, WREN_REPLY);
            break;
        }
        case WREN_REPLY:
            status_read(state, WEL_REPLY);
            break;
        case WEL_REPLY:
            state->status1 = state->response[0];
            if ((state->status1 &
                 (ARRAY_BUSY | WRITE_ENABLE | BLOCK_PROTECT)) != WRITE_ENABLE) {
                fail(state, NX_ERROR_IO);
                break;
            }
            state->pulse = true;
            if (state->operation == NX_MAPPED_STORAGE_PROGRAM)
                (void)issue(state, 0x02U, state->offset + state->completed,
                            state->chunk, true, PULSE_REPLY);
            else
                (void)issue(state, 0x20U, state->offset + state->completed, 0U,
                            false, PULSE_REPLY);
            break;
        case PULSE_REPLY:
            state->pulse_deadline = nx_deadline_after(
                now, state->operation == NX_MAPPED_STORAGE_PROGRAM ? 3100U
                                                                   : 400100U);
            status_read(state, WIP_REPLY);
            break;
        case WIP_REPLY:
            state->status1 = state->response[0];
            if ((state->status1 & ARRAY_BUSY) != 0U) {
                if (nx_deadline_expired(state->pulse_deadline, now))
                    fail(state, NX_ERROR_TIMEOUT);
                else
                    status_read(state, WIP_REPLY);
            } else if ((state->status1 & WRITE_ENABLE) != 0U) {
                fail(state, NX_ERROR_IO);
            } else {
                state->pulse = false;
                state->checked = 0U;
                if (state->cancelled)
                    state->phase = CLEAN_START;
                else
                    read_piece(state, VERIFY_REPLY);
            }
            break;
        case VERIFY_REPLY: {
            size_t count = state->chunk - state->checked;
            if (count > NX_H743_W25Q128_CHECK_BYTES)
                count = NX_H743_W25Q128_CHECK_BYTES;
            for (size_t i = 0U; i < count; ++i) {
                uint8_t expected =
                    state->operation == NX_MAPPED_STORAGE_PROGRAM
                        ? state->input[state->completed + state->checked + i]
                        : UINT8_MAX;
                if (state->response[i] != expected) {
                    fail(state, NX_ERROR_IO);
                    return;
                }
            }
            state->checked += count;
            if (state->checked < state->chunk)
                read_piece(state, VERIFY_REPLY);
            else {
                state->completed += state->chunk;
                state->phase = SELECT_CHUNK;
            }
            break;
        }
        case CLEAN_START:
            status_read(state, CLEAN_REPLY);
            break;
        case CLEAN_REPLY:
            state->status1 = state->response[0];
            if ((state->status1 & ARRAY_BUSY) != 0U)
                status_read(state, CLEAN_REPLY);
            else if ((state->status1 & WRITE_ENABLE) != 0U)
                (void)issue(state, 0x04U, 0U, 0U, false, WRDI_REPLY);
            else {
                state->pulse = false;
                state->phase = state->active ? DONE : IDLE;
                if (!state->active) {
                    state->ready = true;
                    state->result = NX_SUCCESS;
                }
            }
            break;
        case WRDI_REPLY:
            status_read(state, CLEAN_REPLY);
            break;
        default:
            fail(state, NX_ERROR_STATE);
            break;
    }
}
/** \brief A last-map error retains the exact existing map loan. */
static nx_result_t unmap(void* context) {
    nx_h743_w25q128_state_t* state = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!state->mapped || state->active)
        return NX_ERROR_STATE;
    if (!mpu_close(state))
        return NX_ERROR_IO;
    nx_arch_dsb();
    nx_result_t result = abort_step(state);
    if (result != NX_SUCCESS)
        return result;
    result = withdraw(state);
    if (result == NX_SUCCESS)
        state->mapped = false;
    return result;
}
/** \brief Failed map construction cannot return an unowned live aperture. */
static _Noreturn void map_fault(void) {
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}
/** \brief Map only the discovered, idle, uncached CPU-read aperture. */
static nx_result_t map(void* context) {
    nx_h743_w25q128_state_t* state = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!state->ready || state->active || state->mapped || !accessible() ||
        !idle() || !mpu_policy(state, GUARD_DENIED))
        return NX_ERROR_STATE;
    QSPI_CommandTypeDef value = command(0x03U, 0U, 1U);
    QSPI_MemoryMappedTypeDef config = {QSPI_TIMEOUT_COUNTER_DISABLE, 0U};
    HAL_QSPI_SetTimeout(&state->hal, 0U);
    HAL_StatusTypeDef status =
        HAL_QSPI_MemoryMapped(&state->hal, &value, &config);
    uint32_t expected = value.Instruction | value.InstructionMode |
                        value.AddressMode | value.AddressSize | value.DataMode |
                        MAP_MODE;
    if (status == HAL_OK &&
        NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->CCR) == expected &&
        mpu_open(state)) {
        state->mapped = true;
        return NX_SUCCESS;
    }
    if (!mpu_close(state) || abort_step(state) != NX_SUCCESS ||
        withdraw(state) != NX_SUCCESS)
        map_fault();
    return NX_ERROR_IO;
}
/** \brief Validate device range and retained input before command effects. */
static nx_result_t start(void* context, nx_mapped_storage_operation_t operation,
                         size_t offset, const void* data, size_t length) {
    nx_h743_w25q128_state_t* state = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (state->active || state->mapped)
        return NX_ERROR_STATE;
    /* The portable owner has already ACCEPTED this cohort. Even a zero-effect
     * start error needs a drainable receipt, without retaining its input. */
    state->active = true;
    state->input = NULL;
    state->completed = state->checked = state->chunk = 0U;
    state->length = state->offset = 0U;
    state->cancelled = true;
    state->pulse = false;
    state->result = NX_ERROR_STATE;
    state->phase = CLEAN_START;
    if (!state->ready || !accessible() || !idle() ||
        !mpu_policy(state, GUARD_DENIED))
        return NX_ERROR_STATE;
    state->result = NX_ERROR_INVALID;
    if (length == 0U || offset >= NX_H743_W25Q128_BYTES ||
        length > NX_H743_W25Q128_BYTES - offset ||
        (operation != NX_MAPPED_STORAGE_PROGRAM &&
         operation != NX_MAPPED_STORAGE_ERASE))
        return NX_ERROR_INVALID;
    if (operation == NX_MAPPED_STORAGE_PROGRAM) {
        if (!span(data, length) ||
            overlap(data, length, state, sizeof(*state)) ||
            overlap(data, length, NX_H743_W25Q128_APERTURE,
                    NX_H743_W25Q128_BYTES) ||
            overlap(data, length, NX_H743_W25Q128_REGS,
                    sizeof(*NX_H743_W25Q128_REGS)) ||
            overlap(data, length, NX_H743_W25Q128_RCC,
                    sizeof(*NX_H743_W25Q128_RCC)) ||
            overlap(data, length, NX_H743_W25Q128_CORE,
                    sizeof(*NX_H743_W25Q128_CORE)))
            return NX_ERROR_INVALID;
    } else if (data != NULL || offset % NX_H743_W25Q128_ERASE_BYTES != 0U ||
               length % NX_H743_W25Q128_ERASE_BYTES != 0U)
        return NX_ERROR_INVALID;
    state->input = data;
    state->offset = offset;
    state->length = length;
    state->completed = state->checked = state->chunk = 0U;
    state->active = true;
    state->cancelled = state->pulse = false;
    state->result = NX_SUCCESS;
    state->operation = (uint8_t)operation;
    state->phase = CHECK_START;
    return NX_SUCCESS;
}
/** \brief Report a monotonic conservative verified byte prefix. */
static nx_result_t poll(void* context, size_t* completed_bytes) {
    nx_h743_w25q128_state_t* state = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!state->active || completed_bytes == NULL)
        return NX_ERROR_STATE;
    step(state);
    *completed_bytes = state->completed;
    return state->result != NX_SUCCESS
               ? state->result
               : (state->phase == DONE ? NX_SUCCESS : NX_ERROR_BUSY);
}
/** \brief Stop future pulses without releasing the current operation. */
static nx_result_t cancel(void* context) {
    nx_h743_w25q128_state_t* state = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!state->active)
        return NX_ERROR_STATE;
    state->cancelled = true;
    return NX_SUCCESS;
}
/** \brief Actual device/local withdrawal precedes dropping the program input.
 */
static nx_result_t drain(void* context) {
    nx_h743_w25q128_state_t* state = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!state->active)
        return NX_ERROR_STATE;
    if (!mpu_policy(state, GUARD_DENIED))
        return NX_ERROR_IO;
    step(state);
    if (state->phase != DONE)
        return accessible() ? NX_ERROR_BUSY : NX_ERROR_IO;
    nx_result_t result = withdraw(state);
    if (result != NX_SUCCESS)
        return result;
    state->input = NULL;
    state->active = state->cancelled = state->pulse = false;
    state->phase = IDLE;
    state->result = NX_SUCCESS;
    return NX_SUCCESS;
}
/** \brief Adopt only reset controller facts under the external cold lease. */
nx_result_t nx_h743_w25q128_prepare(nx_h743_w25q128_state_t* state,
                                    nx_clock_t clock, uint32_t kernel_hz,
                                    uint32_t serial_hz,
                                    nx_h743_w25q128_mpu_config_t mpu) {
    if (state == NULL || clock.read == NULL || kernel_hz == 0U ||
        serial_hz == 0U || serial_hz > NX_H743_W25Q128_MAX_HZ ||
        serial_hz > kernel_hz ||
        (clock.context != NULL &&
         (uintptr_t)clock.context >= (uintptr_t)state &&
         (uintptr_t)clock.context - (uintptr_t)state < sizeof(*state)))
        return NX_ERROR_INVALID;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (state->owned || state->active || state->mapped)
        return NX_ERROR_BUSY;
    uint32_t mpu_control = NX_H743_W25Q128_READ(&NX_H743_W25Q128_MPU->CTRL);
    uint32_t regions = (NX_H743_W25Q128_READ(&NX_H743_W25Q128_MPU->TYPE) &
                        (uint32_t)MPU_TYPE_DREGION_Msk) >>
                       MPU_TYPE_DREGION_Pos;
    if (mpu.deny_region >= mpu.read_region || mpu.read_region >= 16U)
        return NX_ERROR_INVALID;
    if ((mpu_control & (MPU_CTRL_ENABLE_Msk | MPU_CTRL_HFNMIENA_Msk)) !=
            (MPU_CTRL_ENABLE_Msk | MPU_CTRL_HFNMIENA_Msk) ||
        regions != 16U)
        return NX_ERROR_UNSUPPORTED;
    if (!accessible() || !idle() || NX_H743_W25Q128_IRQ_PENDING() ||
        NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->CR) != 0U ||
        NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->CCR) != 0U ||
        NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->DLR) != 0U ||
        NX_H743_W25Q128_READ(&NX_H743_W25Q128_REGS->AR) != 0U)
        return NX_ERROR_UNSUPPORTED;
    uint32_t divider =
        kernel_hz / serial_hz + (kernel_hz % serial_hz != 0U ? 1U : 0U);
    nx_time_us_t now = clock.read(clock.context);
    if (divider == 0U || divider > 256U || now == NX_DEADLINE_NEVER ||
        now >= NX_DEADLINE_NEVER - 50000U)
        return NX_ERROR_INVALID;
    *state = (nx_h743_w25q128_state_t){
        .clock = clock,
        .owned = true,
        .phase = POWER_GUARD,
        .result = NX_ERROR_BUSY,
        .power_until = now + 5000U,
        .startup_deadline = now + 50000U,
        .mpu_control = mpu_control,
        .mpu_selector = NX_H743_W25Q128_READ(&NX_H743_W25Q128_MPU->RNR),
        .mpu_deny_region = mpu.deny_region,
        .mpu_read_region = mpu.read_region};
    nx_result_t guarded = mpu_claim(state);
    if (guarded != NX_SUCCESS) {
        fail(state, guarded);
        return guarded;
    }
    state->hal.Instance = NX_H743_W25Q128_REGS;
    state->hal.State = HAL_QSPI_STATE_READY;
    state->hal.Lock = HAL_UNLOCKED;
    state->hal.FifoThresholdCallback = no_callback;
    state->hal.StatusMatchCallback = no_callback;
    state->hal.TimeOutCallback = error;
    const HAL_QSPI_CallbackIDTypeDef ids[] = {
        HAL_QSPI_ERROR_CB_ID, HAL_QSPI_ABORT_CB_ID, HAL_QSPI_CMD_CPLT_CB_ID,
        HAL_QSPI_RX_CPLT_CB_ID, HAL_QSPI_TX_CPLT_CB_ID};
    for (size_t i = 0U; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        if (HAL_QSPI_RegisterCallback(&state->hal, ids[i],
                                      i == 0U ? error : completed) != HAL_OK) {
            fail(state, NX_ERROR_IO);
            return NX_ERROR_IO;
        }
    }
    HAL_QSPI_SetTimeout(&state->hal, 0U);
    uint32_t device =
        (23U << QUADSPI_DCR_FSIZE_Pos) | QSPI_CS_HIGH_TIME_2_CYCLE;
    uint32_t control =
        ((divider - 1U) << QUADSPI_CR_PRESCALER_Pos) | QUADSPI_CR_EN;
    if (!checked(&NX_H743_W25Q128_REGS->DCR, device) ||
        !checked(&NX_H743_W25Q128_REGS->CR, control)) {
        fail(state, NX_ERROR_IO);
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
/** \brief One explicit service call creates no asynchronous worker. */
void nx_h743_w25q128_service(nx_h743_w25q128_state_t* state) {
    if (state != NULL && task())
        step(state);
}
/** \brief Discovery does not create a mapping or a request borrow. */
nx_result_t nx_h743_w25q128_status(const nx_h743_w25q128_state_t* state) {
    if (state == NULL || !state->owned)
        return NX_ERROR_STATE;
    return state->ready ? NX_SUCCESS : state->result;
}
/** \brief Export the existing backend only after identity/protection checks. */
nx_result_t nx_h743_w25q128_backend(nx_h743_w25q128_state_t* state,
                                    nx_mapped_storage_backend_t* backend) {
    if (state == NULL || !span(backend, sizeof(*backend)) ||
        (uintptr_t)backend % _Alignof(nx_mapped_storage_backend_t) != 0U ||
        overlap(state, sizeof(*state), backend, sizeof(*backend)))
        return NX_ERROR_INVALID;
    if (!state->owned || !state->ready)
        return NX_ERROR_STATE;
    *backend = (nx_mapped_storage_backend_t){
        state,
        sizeof(*state),
        {NX_H743_W25Q128_APERTURE, NX_H743_W25Q128_BYTES, 1U,
         NX_H743_W25Q128_ERASE_BYTES, NX_MAPPED_STORAGE_CPU_READ},
        map,
        unmap,
        start,
        poll,
        cancel,
        drain};
    return NX_SUCCESS;
}
/** \brief Stop never revokes a generic owner map or request. */
nx_result_t nx_h743_w25q128_stop(nx_h743_w25q128_state_t* state) {
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!state->owned)
        return NX_SUCCESS;
    if (state->active || state->mapped)
        return NX_ERROR_BUSY;
    if (state->pending || !idle()) {
        nx_result_t result = abort_step(state);
        if (result != NX_SUCCESS)
            return result;
    }
    nx_result_t result = withdraw(state);
    if (result != NX_SUCCESS)
        return result;
    if (!checked(&NX_H743_W25Q128_REGS->CR, 0U) ||
        !checked(&NX_H743_W25Q128_REGS->DCR, 0U))
        return NX_ERROR_IO;
    if (!mpu_restore(state))
        return NX_ERROR_IO;
    state->owned = state->ready = false;
    return NX_SUCCESS;
}
