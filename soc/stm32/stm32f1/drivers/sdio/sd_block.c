/**
 * \file            sd_block.c
 * \brief           SD v2 memory sessions on the F103ZE SDIO host.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "dispatch_checks.h"
#include "drivers/sdio/instance.h"
#include "private/hardware.h"
#include "system/platform.h"
#if defined(NX_STM32F1_SDIO_RX_DMA)
#include <string.h>
#endif

#define DATA_PIN          (UINT32_C(1) << 8U)
#define CLOCK_PIN         (UINT32_C(1) << 12U)
#define DETECT_PIN        (UINT32_C(1) << 13U)
#define COMMAND_PIN       (UINT32_C(1) << 2U)
#define DATA_CLOCK_CONFIG UINT32_C(0x004A000A)
#define COMMAND_CONFIG    UINT32_C(0x00000A00)
#define R1_ERRORS         UINT32_C(0xFFFFE008)
#define OCR_3V3           UINT32_C(0x00300000)
#if defined(NX_STM32F1_SDIO_4BIT)
#define WIDE_DATA_PINS   (UINT32_C(7) << 9U)
#define WIDE_DATA_CONFIG UINT32_C(0x0000AAA0)
#endif

enum {
    CARD_CLOCK_SETTLE_US = 2001U,
    POLL_LIMIT = 65535U,
    NEGOTIATION_LIMIT = 4096U,
    STATUS_LIMIT = 2048U,
    CLOSE_LIMIT_US = 1000000U,
    SECTOR_BYTES = 512U,
    SD_APP_CMD = 1U << 5U,
    SD_READY = 1U << 8U,
    SD_STANDBY = 3U,
    SD_TRANSFER = 4U,
    SD_SENDING = 5U,
    SD_RECEIVING = 6U,
    SD_PROGRAMMING = 7U,
    ACCEPT_STANDBY = 1U << SD_STANDBY,
    ACCEPT_TRANSFER = 1U << SD_TRANSFER,
    SD_HCS = 1U << 30U,
};

static bool present(void) {
    return (GPIOC->IDR & DETECT_PIN) == 0U;
}

static nx_result_t available(const nx_stm32f1_sdio_block_t* state,
                             nx_time_us_t deadline) {
    if (state == NULL || deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state->state == NX_BLOCK_ACTIVE)
        return NX_ERROR_BUSY;
    if (state->state == NX_BLOCK_UNAVAILABLE || !nx_stm32f1_system_live() ||
        (RCC->APB2ENR & (RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPDEN)) !=
            (RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPDEN))
        return NX_ERROR_STATE;
    return nx_deadline_expired(deadline, nx_time_now_us()) ? NX_ERROR_TIMEOUT
                                                           : NX_SUCCESS;
}

static nx_result_t short_response(nx_stm32f1_sdio_block_t* state, uint8_t cmd,
                                  uint32_t arg, uint32_t* status,
                                  nx_time_us_t deadline) {
    nx_stm32f1_sdio_reply_t reply;
    nx_result_t result = nx_stm32f1_sdio_command(
        &state->host, cmd, arg, NX_STM32F1_SDIO_SHORT, &reply, deadline);
    if (result == NX_SUCCESS)
        *status = reply.words[0];
    return result;
}

/** \brief Confirm card readiness after any selected data phase. */
static nx_result_t card_ready(nx_stm32f1_sdio_block_t* state, uint32_t expected,
                              nx_time_us_t deadline) {
    for (unsigned attempt = 0U; attempt < STATUS_LIMIT; ++attempt) {
        uint32_t status;
        nx_result_t result = short_response(
            state, 13U, (uint32_t)state->rca << 16U, &status, deadline);
        if (result != NX_SUCCESS)
            return result;
        uint32_t stage = (status >> 9U) & 15U;
        if ((status & R1_ERRORS) != 0U)
            return NX_ERROR_IO;
        if ((status & SD_READY) != 0U && ((1U << stage) & expected) != 0U)
            return NX_SUCCESS;
        if (stage != SD_SENDING && stage != SD_TRANSFER &&
            (!state->allow_write ||
             (stage != SD_RECEIVING && stage != SD_PROGRAMMING)))
            return NX_ERROR_IO;
    }
    return NX_ERROR_TIMEOUT;
}

static nx_result_t startup_clocks(nx_time_us_t deadline) {
    nx_time_us_t ready;
    if (nx_deadline_finite_after(nx_time_now_us(), CARD_CLOCK_SETTLE_US,
                                 &ready) != NX_SUCCESS ||
        ready >= deadline)
        return NX_ERROR_TIMEOUT;
    /* At about 364 kHz even 74 clocks fit within this bounded window. */
    for (unsigned i = 0U; i < POLL_LIMIT; ++i) {
        nx_time_us_t now = nx_time_now_us();
        if (now >= deadline)
            return NX_ERROR_TIMEOUT;
        if (now >= ready)
            return NX_SUCCESS;
    }
    return NX_ERROR_TIMEOUT;
}

static nx_result_t power_up_card(nx_stm32f1_sdio_block_t* state,
                                 nx_time_us_t deadline) {
    nx_result_t result = startup_clocks(deadline);
    if (result == NX_SUCCESS)
        result = nx_stm32f1_sdio_command(&state->host, 0U, 0U,
                                         NX_STM32F1_SDIO_NONE, NULL, deadline);
    uint32_t status;
    if (result == NX_SUCCESS)
        result = short_response(state, 8U, 0x1AAU, &status, deadline);
    if (result != NX_SUCCESS)
        return result;
    if (status != 0x1AAU)
        return NX_ERROR_UNSUPPORTED;

    for (unsigned i = 0U; i < NEGOTIATION_LIMIT; ++i) {
        result = short_response(state, 55U, 0U, &status, deadline);
        if (result != NX_SUCCESS)
            return result;
        if ((status & (R1_ERRORS | SD_APP_CMD)) != SD_APP_CMD)
            return NX_ERROR_UNSUPPORTED;
        nx_stm32f1_sdio_reply_t reply;
        result = nx_stm32f1_sdio_command(&state->host, 41U, SD_HCS | OCR_3V3,
                                         NX_STM32F1_SDIO_OCR, &reply, deadline);
        if (result != NX_SUCCESS)
            return result;
        if ((reply.words[0] & UINT32_C(0x80000000)) != 0U)
            return (reply.words[0] & SD_HCS) != 0U &&
                           (reply.words[0] & OCR_3V3) != 0U &&
                           (reply.words[0] & UINT32_C(0x01000000)) == 0U
                       ? NX_SUCCESS
                       : NX_ERROR_UNSUPPORTED;
    }
    return NX_ERROR_TIMEOUT;
}

#if defined(NX_STM32F1_SDIO_4BIT)
static nx_result_t set_width(nx_stm32f1_sdio_block_t* state, bool wide,
                             nx_time_us_t deadline) {
    uint32_t status;
    const uint32_t address = (uint32_t)state->rca << 16U;
    nx_result_t result = short_response(state, 55U, address, &status, deadline);
    if (result != NX_SUCCESS)
        return result;
    if ((status & (R1_ERRORS | SD_APP_CMD)) != SD_APP_CMD)
        return NX_ERROR_IO;

    /* A lost response cannot disprove a bus change on the card. */
    if (wide)
        state->wide_receipt = true;
    result = short_response(state, 6U, wide ? 2U : 0U, &status, deadline);
    if (result != NX_SUCCESS)
        return result;
    if ((status & R1_ERRORS) != 0U)
        return NX_ERROR_IO;

    result = nx_stm32f1_sdio_set_width(&state->host, wide, deadline);
    if (result == NX_SUCCESS && !wide)
        state->wide_receipt = false;
    return result;
}
#endif

static nx_result_t identify_card(nx_stm32f1_sdio_block_t* state,
                                 nx_time_us_t deadline) {
    nx_stm32f1_sdio_reply_t reply;
    nx_result_t result = nx_stm32f1_sdio_command(
        &state->host, 2U, 0U, NX_STM32F1_SDIO_LONG, &reply, deadline);
    uint32_t status;
    if (result == NX_SUCCESS)
        result = short_response(state, 3U, 0U, &status, deadline);
    if (result != NX_SUCCESS)
        return result;
    if ((status >> 16U) == 0U || (status & 0xE000U) != 0U)
        return NX_ERROR_IO;
    state->rca = (uint16_t)(status >> 16U);
    uint32_t address = (uint32_t)state->rca << 16U;
    result = nx_stm32f1_sdio_command(&state->host, 9U, address,
                                     NX_STM32F1_SDIO_LONG, &reply, deadline);
    if (result != NX_SUCCESS)
        return result;

    /* CSD v2 bits 69:48 carry C_SIZE. READ_BL_LEN is fixed at 512 bytes. */
    if ((reply.words[0] >> 30U) != 1U || ((reply.words[1] >> 16U) & 15U) != 9U)
        return NX_ERROR_UNSUPPORTED;
    uint32_t c_size = ((reply.words[1] & 63U) << 16U) | (reply.words[2] >> 16U);
#if defined(NX_STM32F1_SDIO_4BIT)
    /* SD Physical Layer v9.10 §5.3.3: SDHC CSD v2 capacity bounds. */
    if (state->wide_requested && (c_size < 0x1010U || c_size > 0xFF5FU))
        return NX_ERROR_UNSUPPORTED;
#endif
    /* SDHC CSD v2: class 4, 512-byte writes and no CSD write protection. */
    const bool writable = state->allow_write && c_size >= 0xFFFU &&
                          c_size <= 0xFFFFU &&
                          (reply.words[1] & (UINT32_C(1) << 24U)) != 0U &&
                          ((reply.words[3] >> 22U) & 15U) == 9U &&
                          (reply.words[3] & (UINT32_C(3) << 12U)) == 0U;
    result = short_response(state, 7U, address, &status, deadline);
    if (result != NX_SUCCESS)
        return result;
    if ((status & R1_ERRORS) != 0U)
        return NX_ERROR_IO;
    result = card_ready(state, ACCEPT_TRANSFER, deadline);
#if defined(NX_STM32F1_SDIO_4BIT)
    if (result == NX_SUCCESS && state->wide_requested) {
        result = set_width(state, true, deadline);
        if (result == NX_SUCCESS)
            result = card_ready(state, ACCEPT_TRANSFER, deadline);
    }
#endif
    if (result == NX_SUCCESS)
        state->geometry = (nx_block_geometry_t){
            .block_count = ((uint64_t)c_size + 1U) * UINT64_C(1024),
            .block_bytes = SECTOR_BYTES,
            .buffer_alignment = _Alignof(uint32_t),
            .writable = writable};
    return result;
}

static nx_result_t activate(void* context, nx_time_us_t deadline) {
    nx_stm32f1_sdio_block_t* state = context;
    nx_result_t result = available(state, deadline);
    if (result != NX_SUCCESS)
        return result;
    if (state->state != NX_BLOCK_STOPPED)
        return NX_ERROR_STATE;
    if (!present())
        return NX_ERROR_STATE;

    state->removed = false;
    state->state = NX_BLOCK_ACTIVE;
    result = nx_stm32f1_sdio_open(&state->host, deadline);
    if (result == NX_SUCCESS)
        result = power_up_card(state, deadline);
    if (result == NX_SUCCESS)
        result = identify_card(state, deadline);
    if (result == NX_SUCCESS && !present())
        result = NX_ERROR_IO;
    if (result != NX_SUCCESS)
        state->geometry.block_count = 0U;
    state->state = result == NX_SUCCESS ? NX_BLOCK_READY : NX_BLOCK_UNCERTAIN;
    return result;
}

static bool sector_buffer(const nx_stm32f1_sdio_block_t* state,
                          const void* data, size_t bytes) {
    if (bytes < SECTOR_BYTES ||
        !nx_io_dispatch_span(data, SECTOR_BYTES, _Alignof(uint32_t)) ||
        nx_io_dispatch_overlap(data, SECTOR_BYTES, state, sizeof(*state)))
        return false;
#if !defined(NEXUS_CMSIS_HOST_MODEL)
    /* F103ZET6 has one contiguous 64 KiB CPU SRAM bank. */
    return (uintptr_t)data >= UINT32_C(0x20000000) &&
           (uintptr_t)data <= UINT32_C(0x20010000) - SECTOR_BYTES;
#else
    return !nx_io_dispatch_overlap(
        data, SECTOR_BYTES, (const void*)(uintptr_t)SDIO_BASE, sizeof(*SDIO));
#endif
}

static nx_block_io_result_t transfer_finished(nx_stm32f1_sdio_block_t* state,
                                              nx_result_t result,
                                              nx_time_us_t deadline) {
    if (result == NX_SUCCESS)
        result = card_ready(state, ACCEPT_TRANSFER, deadline);
    if (result == NX_SUCCESS && !present())
        result = NX_ERROR_IO;
    const bool confirmed = result == NX_SUCCESS;
    state->state = confirmed ? NX_BLOCK_READY : NX_BLOCK_UNCERTAIN;
    return (nx_block_io_result_t){result, confirmed ? 1U : 0U};
}

static nx_block_io_result_t read_sector(void* context, uint64_t first,
                                        void* data, size_t bytes,
                                        uint32_t blocks,
                                        nx_time_us_t deadline) {
    nx_stm32f1_sdio_block_t* state = context;
    nx_block_io_result_t outcome = {available(state, deadline), 0U};
    if (outcome.result != NX_SUCCESS)
        return outcome;
    if (state->state != NX_BLOCK_READY || !present())
        return (nx_block_io_result_t){NX_ERROR_STATE, 0U};
    if (blocks != 1U || first >= state->geometry.block_count ||
        !sector_buffer(state, data, bytes))
        return (nx_block_io_result_t){NX_ERROR_INVALID, 0U};

    state->state = NX_BLOCK_ACTIVE;
#if defined(NX_STM32F1_SDIO_4BIT)
    if (state->wide_requested && !nx_stm32f1_sdio_four_bit_ready()) {
        state->state = NX_BLOCK_UNCERTAIN;
        return (nx_block_io_result_t){NX_ERROR_IO, 0U};
    }
#endif
#if defined(NX_STM32F1_SDIO_RX_DMA)
    if (state->host.dma_ready) {
        const nx_result_t received = nx_stm32f1_sdio_read_sector(
            &state->host, (uint32_t)first, state->dma_sector,
            SECTOR_BYTES / sizeof(uint32_t), deadline);
        outcome = transfer_finished(state, received, deadline);
        if (outcome.result == NX_SUCCESS)
            memcpy(data, state->dma_sector, SECTOR_BYTES);
        return outcome;
    }
#endif
    return transfer_finished(
        state,
        nx_stm32f1_sdio_read_sector(&state->host, (uint32_t)first, data,
                                    SECTOR_BYTES / sizeof(uint32_t), deadline),
        deadline);
}

static nx_block_io_result_t write_sector(void* context, uint64_t first,
                                         const void* data, size_t bytes,
                                         uint32_t blocks,
                                         nx_time_us_t deadline) {
    nx_stm32f1_sdio_block_t* state = context;
    const nx_result_t ready = available(state, deadline);
    if (ready != NX_SUCCESS)
        return (nx_block_io_result_t){ready, 0U};
    if (state->state != NX_BLOCK_READY || !present())
        return (nx_block_io_result_t){NX_ERROR_STATE, 0U};
    if (!state->geometry.writable)
        return (nx_block_io_result_t){NX_ERROR_PERMISSION, 0U};
    if (blocks != 1U || first >= state->geometry.block_count ||
        !sector_buffer(state, data, bytes))
        return (nx_block_io_result_t){NX_ERROR_INVALID, 0U};

    state->state = NX_BLOCK_ACTIVE;
#if defined(NX_STM32F1_SDIO_4BIT)
    if (state->wide_requested != nx_stm32f1_sdio_four_bit_ready()) {
        state->state = NX_BLOCK_UNCERTAIN;
        return (nx_block_io_result_t){NX_ERROR_IO, 0U};
    }
#endif
    return transfer_finished(
        state,
        nx_stm32f1_sdio_write_sector(&state->host, (uint32_t)first, data,
                                     SECTOR_BYTES / sizeof(uint32_t), deadline),
        deadline);
}

static const nx_block_geometry_t* geometry(const void* context) {
    const nx_stm32f1_sdio_block_t* state = context;
    return (state->state == NX_BLOCK_READY ||
            state->state == NX_BLOCK_UNCERTAIN) &&
                   state->geometry.block_count != 0U && !state->removed
               ? &state->geometry
               : NULL;
}

static nx_block_state_t session_state(const void* context) {
    const nx_stm32f1_sdio_block_t* state = context;
    return state->state;
}

static nx_result_t sync(void* context, nx_time_us_t deadline) {
    nx_stm32f1_sdio_block_t* state = context;
    nx_result_t result = available(state, deadline);
    if (result != NX_SUCCESS)
        return result;
    if (geometry(state) == NULL || !present())
        return NX_ERROR_STATE;
    state->state = NX_BLOCK_ACTIVE;
    result = nx_stm32f1_sdio_quiesce(&state->host, deadline);
#if defined(NX_STM32F1_SDIO_4BIT)
    if (result == NX_SUCCESS && state->wide_requested &&
        !nx_stm32f1_sdio_four_bit_ready())
        result = NX_ERROR_IO;
#endif
    if (result == NX_SUCCESS)
        result = card_ready(state, ACCEPT_TRANSFER, deadline);
    state->state = result == NX_SUCCESS ? NX_BLOCK_READY : NX_BLOCK_UNCERTAIN;
    return result;
}

static nx_result_t stop(void* context, nx_time_us_t deadline) {
    nx_stm32f1_sdio_block_t* state = context;
    nx_result_t result = available(state, deadline);
    if (result != NX_SUCCESS || state->state == NX_BLOCK_STOPPED)
        return result;

    state->state = NX_BLOCK_ACTIVE;
    if (state->rca != 0U && !state->removed) {
        result = nx_stm32f1_sdio_quiesce(&state->host, deadline);
        uint32_t accepted = ACCEPT_TRANSFER | ACCEPT_STANDBY;
#if defined(NX_STM32F1_SDIO_4BIT)
        if (state->wide_receipt)
            accepted = ACCEPT_TRANSFER;
#endif
        if (result == NX_SUCCESS)
            result = card_ready(state, accepted, deadline);
#if defined(NX_STM32F1_SDIO_4BIT)
        if (result == NX_SUCCESS && state->wide_receipt)
            result = set_width(state, false, deadline);
#endif
        if (result == NX_SUCCESS)
            result = nx_stm32f1_sdio_command(
                &state->host, 7U, 0U, NX_STM32F1_SDIO_NONE, NULL, deadline);
        if (result == NX_SUCCESS)
            result = card_ready(state, ACCEPT_STANDBY, deadline);
        if (result == NX_SUCCESS)
            state->rca = 0U;
    }
    if (result == NX_SUCCESS)
        result = nx_stm32f1_sdio_close(&state->host, deadline);
    if (result == NX_SUCCESS) {
#if defined(NX_STM32F1_SDIO_4BIT)
        state->wide_receipt = false;
#endif
        state->rca = 0U;
        state->geometry.block_count = 0U;
        state->state = NX_BLOCK_STOPPED;
    } else {
        state->state = NX_BLOCK_UNCERTAIN;
    }
    return result;
}

static nx_result_t media_removed(void* context) {
    nx_stm32f1_sdio_block_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state->state == NX_BLOCK_ACTIVE)
        return NX_ERROR_BUSY;
    if (state->state == NX_BLOCK_UNAVAILABLE)
        return NX_ERROR_STATE;
    if ((RCC->APB2ENR & RCC_APB2ENR_IOPCEN) == 0U || present())
        return NX_ERROR_STATE;
    state->removed = true;
    state->geometry.block_count = 0U;
    if (state->state != NX_BLOCK_STOPPED)
        state->state = NX_BLOCK_UNCERTAIN;
    return NX_SUCCESS;
}

const nx_block_ops_t nx_stm32f1_sdio_block_ops = {.geometry = geometry,
                                                  .state = session_state,
                                                  .activate = activate,
                                                  .read = read_sector,
                                                  .sync = sync,
                                                  .stop = stop,
                                                  .media_removed =
                                                      media_removed};

const nx_block_ops_t nx_stm32f1_sdio_block_write_ops = {.geometry = geometry,
                                                        .state = session_state,
                                                        .activate = activate,
                                                        .read = read_sector,
                                                        .write = write_sector,
                                                        .sync = sync,
                                                        .stop = stop,
                                                        .media_removed =
                                                            media_removed};

#if defined(NX_STM32F1_SDIO_4BIT)
static nx_result_t prepare_block(nx_stm32f1_sdio_block_t* state, bool wide)
#else
nx_result_t nx_stm32f1_sdio_block_prepare(nx_stm32f1_sdio_block_t* state)
#endif
{
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state->state != NX_BLOCK_UNAVAILABLE ||
        state->port_c.registers != NULL || state->port_d.registers != NULL ||
        !nx_stm32f1_system_live())
        return NX_ERROR_STATE;
    if ((RCC->AHBENR & RCC_AHBENR_SDIOEN) != 0U)
        return NX_ERROR_BUSY;

    uint16_t data_pins = DATA_PIN | CLOCK_PIN | DETECT_PIN;
    uint32_t pad_config = DATA_CLOCK_CONFIG;
#if defined(NX_STM32F1_SDIO_4BIT)
    if (wide) {
        data_pins |= WIDE_DATA_PINS;
        pad_config |= WIDE_DATA_CONFIG;
    }
#endif
    nx_result_t result =
        nx_stm32f1_pads_capture(&state->port_c, GPIOC, data_pins);
    if (result == NX_SUCCESS) {
        /* ES0340: SDIO also conflicts with AF outputs on unused PC9/PC10. */
        const uint32_t config = GPIOC->CRH;
        if (((config & 0x80U) != 0U && (config & 0x30U) != 0U) ||
            ((config & 0x800U) != 0U && (config & 0x300U) != 0U))
            return NX_ERROR_BUSY;
        result = nx_stm32f1_pads_capture(&state->port_d, GPIOD, COMMAND_PIN);
    }
    if (result == NX_SUCCESS)
        result = nx_stm32f1_pads_apply(&state->port_c, 0U, pad_config,
                                       state->port_c.latch);
    if (result == NX_SUCCESS)
        result = nx_stm32f1_pads_apply(&state->port_d, COMMAND_CONFIG, 0U,
                                       state->port_d.latch);
    if (result == NX_SUCCESS) {
        state->allow_write = false;
#if defined(NX_STM32F1_SDIO_4BIT)
        state->wide_requested = wide;
        state->wide_receipt = false;
#endif
        state->state = NX_BLOCK_STOPPED;
    }
    return result;
}

#if defined(NX_STM32F1_SDIO_4BIT)
nx_result_t nx_stm32f1_sdio_block_prepare(nx_stm32f1_sdio_block_t* state) {
    return prepare_block(state, false);
}

nx_result_t
nx_stm32f1_sdio_block_prepare_four_bit(nx_stm32f1_sdio_block_t* state) {
    return prepare_block(state, true);
}

nx_result_t
nx_stm32f1_sdio_block_prepare_four_bit_write(nx_stm32f1_sdio_block_t* state) {
    nx_result_t result = nx_stm32f1_sdio_block_prepare_four_bit(state);
    if (result == NX_SUCCESS)
        state->allow_write = true;
    return result;
}
#endif

nx_result_t
nx_stm32f1_sdio_block_prepare_write(nx_stm32f1_sdio_block_t* state) {
    nx_result_t result = nx_stm32f1_sdio_block_prepare(state);
    if (result == NX_SUCCESS)
        state->allow_write = true;
    return result;
}

#if defined(NX_STM32F1_SDIO_RX_DMA)
nx_result_t
nx_stm32f1_sdio_block_prepare_four_bit_dma(nx_stm32f1_sdio_block_t* state) {
    nx_result_t result = nx_stm32f1_sdio_block_prepare_four_bit(state);
    return result == NX_SUCCESS ? nx_stm32f1_sdio_dma_prepare(&state->host)
                                : result;
}
#endif

nx_result_t nx_stm32f1_sdio_block_close(nx_stm32f1_sdio_block_t* state) {
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state->state != NX_BLOCK_UNAVAILABLE &&
        state->state != NX_BLOCK_STOPPED) {
        nx_time_us_t deadline;
        if (nx_deadline_finite_after(nx_time_now_us(), CLOSE_LIMIT_US,
                                     &deadline) != NX_SUCCESS)
            return NX_ERROR_TIMEOUT;
        nx_result_t withdrawn = stop(state, deadline);
        if (withdrawn != NX_SUCCESS)
            return withdrawn;
    }

    state->state = NX_BLOCK_UNAVAILABLE;
#if defined(NX_STM32F1_SDIO_RX_DMA)
    const nx_result_t released = nx_stm32f1_sdio_dma_release(&state->host);
    if (released != NX_SUCCESS)
        return released;
#endif
    nx_result_t result = nx_stm32f1_pads_restore(&state->port_d);
    if (result == NX_SUCCESS)
        result = nx_stm32f1_pads_restore(&state->port_c);
    return result;
}
