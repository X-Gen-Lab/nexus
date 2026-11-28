/**
 * \file            crc.c
 * \brief           STM32F103 fixed-polynomial word CRC.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "dispatch_checks.h"
#include "instance.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_crc.h"
#include "system/platform.h"

#ifndef NX_STM32F1_CRC_FEED
#define NX_STM32F1_CRC_FEED(reg, value) LL_CRC_FeedData32((reg), (value))
#endif

/** \brief The assembly exclusively owns the calculator, independent of RCC. */
nx_result_t nx_stm32f1_crc_prepare(nx_stm32f1_crc_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->live || state->gate_added || !nx_stm32f1_system_live()) {
        return NX_ERROR_STATE;
    }

    const uint32_t gate = RCC_AHBENR_CRCEN;
    state->gate_added = (RCC->AHBENR & gate) == 0U;
    if (state->gate_added &&
        !nx_stm32f1_write_masked(&RCC->AHBENR, gate, gate)) {
        return NX_ERROR_IO;
    }
    state->live = true;
    return NX_SUCCESS;
}

/** \brief Withdraw only the clock this instance enabled. */
nx_result_t nx_stm32f1_crc_close(nx_stm32f1_crc_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    state->live = false;
    if (state->gate_added &&
        !nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_CRCEN, 0U)) {
        return NX_ERROR_IO;
    }
    state->gate_added = false;
    return NX_SUCCESS;
}

/** \brief Match the live deadline domain and report a vanished AHB gate. */
static nx_result_t progress(nx_time_us_t deadline) {
    if ((RCC->AHBENR & RCC_AHBENR_CRCEN) == 0U) {
        return NX_ERROR_IO;
    }
    return nx_deadline_expired(deadline, nx_time_now_us()) ? NX_ERROR_TIMEOUT
                                                           : NX_SUCCESS;
}

/** \brief Reject calculator and clock aliases before any hardware effects. */
static bool independent(const nx_stm32f1_crc_t* state, const void* data,
                        size_t bytes) {
    return !nx_io_dispatch_overlap(data, bytes, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(data, bytes, CRC, sizeof(*CRC)) &&
           !nx_io_dispatch_overlap(data, bytes, (const void*)&RCC->AHBENR,
                                   sizeof(RCC->AHBENR));
}

/** \brief Observe the reset seed. CRC_CR.RESET is write-only in RM0008. */
static nx_result_t reset_crc(nx_time_us_t deadline) {
    NX_STM32F1_WRITE(&CRC->CR, CRC_CR_RESET);
    nx_arch_dsb();
    for (unsigned attempt = 0U; attempt < 8U; ++attempt) {
        nx_result_t status = progress(deadline);
        if (status != NX_SUCCESS) {
            return status;
        }
        if (LL_CRC_ReadData32(CRC) == UINT32_MAX) {
            return NX_SUCCESS;
        }
    }
    return NX_ERROR_IO;
}

/** \brief FIFO-free, ordered 32-bit writes with bounded deadline checks. */
static nx_result_t compute(void* context, const uint32_t* words, size_t count,
                           nx_time_us_t deadline, uint32_t* output) {
    const nx_stm32f1_crc_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->live) {
        return NX_ERROR_STATE;
    }
    if (!independent(state, output, sizeof(*output)) ||
        (count != 0U && !independent(state, words, count * sizeof(*words)))) {
        return NX_ERROR_INVALID;
    }
    nx_result_t status = progress(deadline);
    if (status == NX_SUCCESS) {
        status = reset_crc(deadline);
    }
    for (size_t offset = 0U; status == NX_SUCCESS && offset < count;) {
        size_t chunk = count - offset < 16U ? count - offset : 16U;
        for (size_t index = 0U; index < chunk; ++index) {
            NX_STM32F1_CRC_FEED(CRC, words[offset++]);
        }
        status = progress(deadline);
    }
    if (status == NX_SUCCESS) {
        const uint32_t value = LL_CRC_ReadData32(CRC);
        status = progress(deadline);
        if (status == NX_SUCCESS) {
            *output = value;
        }
    }
    return status;
}

const nx_crc_ops_t nx_stm32f1_crc_ops = {.compute = compute};
