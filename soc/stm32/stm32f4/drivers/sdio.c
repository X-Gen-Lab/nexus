/**
 * \file            sdio.c
 * \brief           Exact SDIO polling block ownership and bounded protocol
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_sdio.h"
#include "stm32f4xx_ll_sdmmc.h"
#include <string.h>

#ifndef NX_SDIO_READ
#define NX_SDIO_READ(reg) (reg)
#endif
#ifndef NX_SDIO_WRITE
#define NX_SDIO_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif
#ifndef NX_SDIO_FIFO_READ
#define NX_SDIO_FIFO_READ(registers) SDIO_ReadFIFO(registers)
#endif
#ifndef NX_SDIO_FIFO_WRITE
#define NX_SDIO_FIFO_WRITE(registers, word) SDIO_WriteFIFO(registers, word)
#endif

static bool allowed_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

static bool cpu_storage(const void* pointer, size_t size, size_t alignment) {
    uintptr_t start = (uintptr_t)pointer;
    if (pointer == NULL || start % alignment != 0U ||
        size > UINTPTR_MAX - start)
        return false;
    uintptr_t end = start + size;
    return !(start < UINT32_C(0x60000000) && end > UINT32_C(0x40000000)) &&
           !(start < UINT32_C(0xF0000000) && end > UINT32_C(0xE0000000));
}

static bool valid_config(const nx_stm32_sdio_config_t* config) {
    if (!cpu_storage(config, sizeof(*config),
                     _Alignof(nx_stm32_sdio_config_t)) ||
        config->registers != SDIO ||
        !cpu_storage(config->pins, 6U * sizeof(*config->pins),
                     _Alignof(nx_stm32_platform_pin_config_t)) ||
        (config->bus_width != 1U && config->bus_width != 4U) ||
        config->max_blocks == 0U || config->max_blocks > 32U ||
        config->poll_budget == 0U || config->poll_budget > 1000000U ||
        config->stop_timeout_us == 0U)
        return false;
    for (size_t i = 0U; i < 6U; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        if (pin->registers != (i == 5U ? GPIOD : GPIOC) ||
            pin->port_index != (i == 5U ? 3U : 2U) ||
            pin->pin != (i == 5U ? 2U : 8U + i) ||
            !cpu_storage(pin->snapshot, sizeof(uint16_t), _Alignof(uint16_t)))
            return false;
        for (size_t j = 0U; j < i; ++j)
            if (pin->snapshot == config->pins[j].snapshot)
                return false;
    }
    return true;
}

static bool live_clock(void) {
    const uint32_t pll_mask = RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN |
                              RCC_PLLCFGR_PLLP | RCC_PLLCFGR_PLLSRC |
                              RCC_PLLCFGR_PLLQ;
    const uint32_t pll =
        RCC_PLLCFGR_PLLSRC_HSE | 8U | (336U << 6U) | (7U << 24U);
    return (NX_SDIO_READ(RCC->CR) & (RCC_CR_HSERDY | RCC_CR_PLLRDY)) ==
               (RCC_CR_HSERDY | RCC_CR_PLLRDY) &&
           (NX_SDIO_READ(RCC->PLLCFGR) & pll_mask) == pll &&
           (NX_SDIO_READ(RCC->CFGR) &
            (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2)) ==
               (RCC_CFGR_SWS_PLL | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2);
}

static bool host_idle(void) {
    return (NX_SDIO_READ(SDIO->STA) &
            (SDIO_STA_CMDACT | SDIO_STA_TXACT | SDIO_STA_RXACT)) == 0U;
}

static const nx_block_geometry_t* block_geometry(const void* context) {
    const nx_stm32_sdio_state_t* state = context;
    return state->geometry_valid ? &state->geometry : NULL;
}

static nx_block_state_t block_state(const void* context) {
    const nx_stm32_sdio_state_t* state = context;
    return state->infrastructure ? state->state : NX_BLOCK_UNAVAILABLE;
}

/* One budget spans every command, FIFO word and ready observation in a call.
 * This also bounds failure when the monotonic source stops advancing. */
typedef struct {
    nx_time_us_t deadline;
    uint32_t remaining;
    nx_time_us_t observed;
} sdio_poll_t;

typedef enum {
    RESPONSE_NONE,
    RESPONSE_R1,
    RESPONSE_R2,
    RESPONSE_R3,
    RESPONSE_R6,
    RESPONSE_R7
} sdio_response_t;

#define COMMAND_FLAGS                                                          \
    (SDIO_STA_CCRCFAIL | SDIO_STA_CTIMEOUT | SDIO_STA_CMDREND |                \
     SDIO_STA_CMDSENT)
#define DATA_ERRORS                                                            \
    (SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_TXUNDERR |               \
     SDIO_STA_RXOVERR | SDIO_STA_STBITERR)
#define DATA_FLAGS  (DATA_ERRORS | SDIO_STA_DATAEND | SDIO_STA_DBCKEND)
#define CARD_ERRORS UINT32_C(0xFFFFE008)

static nx_result_t poll_next(sdio_poll_t* poll) {
    if (poll->remaining == 0U)
        return NX_ERROR_TIMEOUT;
    --poll->remaining;
    poll->observed = nx_time_now_us();
    return poll->observed >= poll->deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}

static nx_result_t control_settled(sdio_poll_t* poll) {
    /* RM0090 31.9.1/2/9 require 3 SDIOCLK + 2 PCLK2 cycles between
     * clock-domain control writes, plus 7 HCLK cycles for POWER. At the
     * reviewed 48/84/168 MHz profile one measured microsecond exceeds all
     * three bounds. No uncalibrated delay loop or hidden HAL tick is used. */
    nx_result_t result = poll_next(poll);
    if (result != NX_SUCCESS)
        return result;
    nx_time_us_t settled;
    result = nx_deadline_finite_after(poll->observed, 1U, &settled);
    if (result != NX_SUCCESS)
        return result;
    do {
        result = poll_next(poll);
        if (result != NX_SUCCESS)
            return result;
    } while (poll->observed < settled);
    return NX_SUCCESS;
}

static nx_result_t command(uint32_t index, uint32_t argument,
                           sdio_response_t response, sdio_poll_t* poll,
                           uint32_t* value) {
    nx_result_t result = poll_next(poll);
    if (result != NX_SUCCESS)
        return result;
    if ((NX_SDIO_READ(SDIO->STA) & SDIO_STA_CMDACT) != 0U)
        return NX_ERROR_BUSY;
    NX_SDIO_WRITE(SDIO->ICR, COMMAND_FLAGS);
    SDIO_CmdInitTypeDef selected = {
        .Argument = argument,
        .CmdIndex = index,
        .Response = response == RESPONSE_NONE ? SDIO_RESPONSE_NO
                    : response == RESPONSE_R2 ? SDIO_RESPONSE_LONG
                                              : SDIO_RESPONSE_SHORT,
        .WaitForInterrupt = SDIO_WAIT_NO,
        .CPSM = SDIO_CPSM_ENABLE,
    };
    if (SDIO_SendCommand(SDIO, &selected) != HAL_OK)
        return NX_ERROR_IO;
    uint32_t status;
    do {
        result = poll_next(poll);
        if (result != NX_SUCCESS)
            return result;
        status = NX_SDIO_READ(SDIO->STA);
    } while ((status & COMMAND_FLAGS) == 0U ||
             (status & SDIO_STA_CMDACT) != 0U);
    uint32_t reply = NX_SDIO_READ(SDIO->RESP1);
    NX_SDIO_WRITE(SDIO->ICR, status & COMMAND_FLAGS);
    if ((status & SDIO_STA_CTIMEOUT) != 0U)
        return NX_ERROR_TIMEOUT;
    /* R3 carries OCR and has no valid CRC field; CCRCFAIL is its normal
     * completion on this controller. Every CRC-bearing response stays strict.
     */
    if ((status & SDIO_STA_CCRCFAIL) != 0U && response != RESPONSE_R3)
        return NX_ERROR_IO;
    if (response != RESPONSE_NONE && response != RESPONSE_R2 &&
        response != RESPONSE_R3 && (NX_SDIO_READ(SDIO->RESPCMD) & 63U) != index)
        return NX_ERROR_IO;
    if (value != NULL)
        *value = reply;
    if (response == RESPONSE_R1 && (reply & CARD_ERRORS) != 0U)
        return NX_ERROR_IO;
    if (response == RESPONSE_R6 && (reply & UINT32_C(0xE000)) != 0U)
        return NX_ERROR_IO;
    return NX_SUCCESS;
}

static nx_result_t app_command(nx_stm32_sdio_state_t* state, uint32_t index,
                               uint32_t argument, sdio_response_t response,
                               sdio_poll_t* poll, uint32_t* value) {
    uint32_t prefix = 0U;
    nx_result_t result =
        command(55U, (uint32_t)state->rca << 16U, RESPONSE_R1, poll, &prefix);
    if (result != NX_SUCCESS)
        return result;
    if ((prefix & (1U << 5U)) == 0U)
        return NX_ERROR_UNSUPPORTED;
    return command(index, argument, response, poll, value);
}

static nx_result_t host_withdraw(sdio_poll_t* poll) {
    nx_result_t settled = control_settled(poll);
    if (settled != NX_SUCCESS)
        return settled;
    NX_SDIO_WRITE(SDIO->DCTRL, 0U);
    NX_SDIO_WRITE(SDIO->CMD, 0U);
    nx_arch_dsb();
    for (;;) {
        nx_result_t result = poll_next(poll);
        if (result != NX_SUCCESS)
            return result;
        if (host_idle() && NX_SDIO_READ(SDIO->DCTRL) == 0U &&
            (NX_SDIO_READ(SDIO->CMD) & SDIO_CMD_CPSMEN) == 0U) {
            NX_SDIO_WRITE(SDIO->ICR, DATA_FLAGS | COMMAND_FLAGS);
            return NX_SUCCESS;
        }
    }
}

static nx_result_t card_ready(nx_stm32_sdio_state_t* state, sdio_poll_t* poll,
                              bool stopping) {
    if (!state->selected || state->rca == 0U)
        return NX_ERROR_STATE;
    for (;;) {
        uint32_t status = 0U;
        nx_result_t result = command(13U, (uint32_t)state->rca << 16U,
                                     RESPONSE_R1, poll, &status);
        if (result != NX_SUCCESS)
            return result;
        uint32_t card_state = (status >> 9U) & 15U;
        if ((status & (1U << 8U)) != 0U &&
            (card_state == 4U || (stopping && card_state == 3U))) {
            state->programming_pending = false;
            if (card_state == 3U)
                state->selected = false;
            return host_idle() ? NX_SUCCESS : NX_ERROR_BUSY;
        }
        if (card_state == 5U || card_state == 6U) {
            /* Even a single-block transfer can leave the card sending or
             * receiving after host abort. CMD12 is recovery, never a claim
             * that this profile supports multi-block command streams. */
            result = command(12U, 0U, RESPONSE_R1, poll, NULL);
            if (result != NX_SUCCESS)
                return result;
        }
    }
}

static nx_result_t data_configure(uint32_t bytes, bool reading,
                                  sdio_poll_t* poll) {
    if ((NX_SDIO_READ(SDIO->DCTRL) & SDIO_DCTRL_DTEN) != 0U || !host_idle())
        return NX_ERROR_BUSY;
    nx_result_t result = control_settled(poll);
    if (result != NX_SUCCESS)
        return result;
    NX_SDIO_WRITE(SDIO->ICR, DATA_FLAGS);
    const SDIO_DataInitTypeDef data = {
        .DataTimeOut = UINT32_C(0x00FFFFFF),
        .DataLength = bytes,
        .DataBlockSize =
            bytes == 8U ? SDIO_DATABLOCK_SIZE_8B : SDIO_DATABLOCK_SIZE_512B,
        .TransferDir =
            reading ? SDIO_TRANSFER_DIR_TO_SDIO : SDIO_TRANSFER_DIR_TO_CARD,
        .TransferMode = SDIO_TRANSFER_MODE_BLOCK,
        .DPSM = SDIO_DPSM_ENABLE,
    };
    SDIO_DataInitTypeDef selected = data;
    return SDIO_ConfigData(SDIO, &selected) == HAL_OK ? NX_SUCCESS
                                                      : NX_ERROR_IO;
}

static nx_result_t fifo_transfer(void* data, uint32_t words, bool reading,
                                 sdio_poll_t* poll) {
    uint32_t copied = 0U;
    uint8_t* buffer = data;
    for (;;) {
        nx_result_t result = poll_next(poll);
        if (result != NX_SUCCESS)
            return result;
        uint32_t status = NX_SDIO_READ(SDIO->STA);
        if ((status & DATA_ERRORS) != 0U)
            return (status & SDIO_STA_DTIMEOUT) != 0U ? NX_ERROR_TIMEOUT
                                                      : NX_ERROR_IO;
        if (reading && (status & SDIO_STA_RXDAVL) != 0U && copied < words) {
            uint32_t word = NX_SDIO_FIFO_READ(SDIO);
            memcpy(buffer + copied * 4U, &word, sizeof(word));
            ++copied;
        } else if (!reading && (status & SDIO_STA_TXFIFOHE) != 0U &&
                   copied < words) {
            uint32_t word;
            memcpy(&word, buffer + copied * 4U, sizeof(word));
            if (NX_SDIO_FIFO_WRITE(SDIO, &word) != HAL_OK)
                return NX_ERROR_IO;
            ++copied;
        }
        /* DATAEND only means DCOUNT reached zero. It can precede CRC
         * acceptance and the final CPU FIFO drain; DBCKEND proves CRC. */
        if ((status & (SDIO_STA_DATAEND | SDIO_STA_DBCKEND)) ==
                (SDIO_STA_DATAEND | SDIO_STA_DBCKEND) &&
            (status & (SDIO_STA_TXACT | SDIO_STA_RXACT)) == 0U) {
            /* CRC/counter completion is independent of the APB FIFO tail.
             * Drain only the remaining selected extent, re-observing flags;
             * never consume an extra word into caller storage. */
            status = NX_SDIO_READ(SDIO->STA);
            if ((status & DATA_ERRORS) != 0U)
                return (status & SDIO_STA_DTIMEOUT) != 0U ? NX_ERROR_TIMEOUT
                                                          : NX_ERROR_IO;
            if (reading && copied < words && (status & SDIO_STA_RXDAVL) != 0U)
                continue;
            if (copied != words || NX_SDIO_READ(SDIO->DCOUNT) != 0U ||
                (reading && (status & SDIO_STA_RXDAVL) != 0U))
                return NX_ERROR_IO;
            result = control_settled(poll);
            if (result != NX_SUCCESS)
                return result;
            NX_SDIO_WRITE(SDIO->DCTRL, 0U);
            nx_arch_dsb();
            if (NX_SDIO_READ(SDIO->DCTRL) != 0U || !host_idle())
                return NX_ERROR_IO;
            NX_SDIO_WRITE(SDIO->ICR, status & DATA_FLAGS);
            return NX_SUCCESS;
        }
    }
}

static void response_bytes(uint8_t bytes[16]) {
    const uint32_t words[4] = {
        NX_SDIO_READ(SDIO->RESP1), NX_SDIO_READ(SDIO->RESP2),
        NX_SDIO_READ(SDIO->RESP3), NX_SDIO_READ(SDIO->RESP4)};
    for (size_t i = 0U; i < 4U; ++i)
        for (size_t j = 0U; j < 4U; ++j)
            bytes[i * 4U + j] = (uint8_t)(words[i] >> (24U - 8U * j));
}

static nx_result_t decode_csd(nx_stm32_sdio_state_t* state) {
    uint8_t bytes[16];
    response_bytes(bytes);
    uint32_t structure = bytes[0] >> 6U;
    uint64_t blocks;
    if (structure == 1U && state->high_capacity) {
        uint32_t size = ((uint32_t)(bytes[7] & 63U) << 16U) |
                        ((uint32_t)bytes[8] << 8U) | bytes[9];
        blocks = ((uint64_t)size + 1U) * UINT64_C(1024);
    } else if (structure == 0U && !state->high_capacity) {
        uint32_t size = ((uint32_t)(bytes[6] & 3U) << 10U) |
                        ((uint32_t)bytes[7] << 2U) | (bytes[8] >> 6U);
        uint32_t mult = ((uint32_t)(bytes[9] & 3U) << 1U) | (bytes[10] >> 7U);
        uint32_t read_bits = bytes[5] & 15U;
        if (read_bits < 9U || read_bits > 11U)
            return NX_ERROR_UNSUPPORTED;
        uint64_t capacity = ((uint64_t)size + 1U) << (mult + 2U + read_bits);
        blocks = capacity / 512U;
        if (blocks > UINT64_C(8388608))
            return NX_ERROR_UNSUPPORTED;
    } else {
        return NX_ERROR_UNSUPPORTED;
    }
    if (blocks == 0U || blocks > UINT64_C(4294967296))
        return NX_ERROR_UNSUPPORTED;
    state->geometry = (nx_block_geometry_t){
        .block_count = blocks,
        .block_bytes = 512U,
        .buffer_alignment = 4U,
        .writable = (bytes[14] & 48U) == 0U,
    };
    return NX_SUCCESS;
}

static nx_result_t configured_clock(uint32_t divider, uint32_t width,
                                    sdio_poll_t* poll) {
    const SDIO_InitTypeDef clock = {
        .ClockEdge = SDIO_CLOCK_EDGE_RISING,
        .ClockBypass = SDIO_CLOCK_BYPASS_DISABLE,
        .ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE,
        .BusWide = width,
        .HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE,
        .ClockDiv = divider,
    };
    nx_result_t result = control_settled(poll);
    if (result != NX_SUCCESS)
        return result;
    if (SDIO_Init(SDIO, clock) != HAL_OK)
        return NX_ERROR_IO;
    result = control_settled(poll);
    if (result != NX_SUCCESS)
        return result;
    const uint32_t expected = divider | width | SDIO_CLKCR_CLKEN;
    NX_SDIO_WRITE(SDIO->CLKCR, expected);
    nx_arch_dsb();
    return NX_SDIO_READ(SDIO->CLKCR) == expected ? NX_SUCCESS : NX_ERROR_IO;
}

static nx_result_t initialize_card(nx_stm32_sdio_state_t* state,
                                   sdio_poll_t* poll) {
    nx_result_t result = control_settled(poll);
    if (result != NX_SUCCESS)
        return result;
    state->protocol_started = true;
    if (SDIO_PowerState_ON(SDIO) != HAL_OK ||
        NX_SDIO_READ(SDIO->POWER) != SDIO_POWER_PWRCTRL)
        return NX_ERROR_IO;
    result = configured_clock(118U, SDIO_BUS_WIDE_1B, poll);
    if (result != NX_SUCCESS)
        return result;
    /* At 400 kHz this exceeds 74 clocks and the power-on settling interval.
     * It is charged to the same finite deadline/budget as card discovery. */
    nx_time_us_t ready_at;
    result = nx_deadline_finite_after(nx_time_now_us(), 2000U, &ready_at);
    if (result != NX_SUCCESS)
        return result;
    do {
        result = poll_next(poll);
        if (result != NX_SUCCESS)
            return result;
    } while (poll->observed < ready_at);
    result = command(0U, 0U, RESPONSE_NONE, poll, NULL);
    if (result != NX_SUCCESS)
        return result;
    uint32_t echo = 0U;
    result = command(8U, 0x1AAU, RESPONSE_R7, poll, &echo);
    bool illegal = (echo & CARD_ERRORS) == (1U << 22U);
    bool version2 = result == NX_SUCCESS && !illegal;
    if (result != NX_SUCCESS && result != NX_ERROR_TIMEOUT)
        return result;
    if (version2 && echo != 0x1AAU)
        return NX_ERROR_UNSUPPORTED;
    uint32_t ocr = 0U;
    do {
        result = app_command(
            state, 41U, UINT32_C(0x00FF8000) | (version2 ? (1U << 30U) : 0U),
            RESPONSE_R3, poll, &ocr);
        if (result != NX_SUCCESS)
            return result;
    } while ((ocr & (1U << 31U)) == 0U);
    if ((ocr & UINT32_C(0x00FF8000)) == 0U ||
        (!version2 && (ocr & (1U << 30U)) != 0U))
        return NX_ERROR_UNSUPPORTED;
    state->high_capacity = (ocr & (1U << 30U)) != 0U;
    result = command(2U, 0U, RESPONSE_R2, poll, NULL);
    if (result != NX_SUCCESS)
        return result;
    state->cid[0] = NX_SDIO_READ(SDIO->RESP1);
    state->cid[1] = NX_SDIO_READ(SDIO->RESP2);
    state->cid[2] = NX_SDIO_READ(SDIO->RESP3);
    state->cid[3] = NX_SDIO_READ(SDIO->RESP4);
    uint32_t rca = 0U;
    result = command(3U, 0U, RESPONSE_R6, poll, &rca);
    if (result != NX_SUCCESS)
        return result;
    state->rca = (uint16_t)(rca >> 16U);
    if (state->rca == 0U)
        return NX_ERROR_IO;
    result = command(9U, (uint32_t)state->rca << 16U, RESPONSE_R2, poll, NULL);
    if (result == NX_SUCCESS)
        result = decode_csd(state);
    if (result != NX_SUCCESS)
        return result;
    /* Selection may have reached the card even if the response is lost. */
    state->selected = true;
    result = command(7U, (uint32_t)state->rca << 16U, RESPONSE_R1, poll, NULL);
    if (result != NX_SUCCESS)
        return result;
    result = card_ready(state, poll, false);
    if (result != NX_SUCCESS)
        return result;
    if (!state->high_capacity) {
        result = command(16U, 512U, RESPONSE_R1, poll, NULL);
        if (result != NX_SUCCESS)
            return result;
    }
    if (state->config->bus_width == 4U) {
        uint32_t scr[2] = {0U, 0U};
        result = data_configure(8U, true, poll);
        if (result == NX_SUCCESS)
            result = app_command(state, 51U, 0U, RESPONSE_R1, poll, NULL);
        if (result == NX_SUCCESS)
            result = fifo_transfer(scr, 2U, true, poll);
        if (result != NX_SUCCESS)
            return result;
        if ((((uint8_t*)scr)[1] & 4U) == 0U)
            return NX_ERROR_UNSUPPORTED;
        result = app_command(state, 6U, 2U, RESPONSE_R1, poll, NULL);
        if (result != NX_SUCCESS)
            return result;
    }
    return configured_clock(2U,
                            state->config->bus_width == 4U ? SDIO_BUS_WIDE_4B
                                                           : SDIO_BUS_WIDE_1B,
                            poll);
}

static nx_result_t operation_valid(nx_stm32_sdio_state_t* state,
                                   nx_time_us_t deadline) {
    if (!allowed_context())
        return NX_ERROR_CONTEXT;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (!state->infrastructure || state->config == NULL)
        return NX_ERROR_STATE;
    if (state->state == NX_BLOCK_ACTIVE)
        return NX_ERROR_BUSY;
    if (!live_clock() ||
        (NX_SDIO_READ(RCC->APB2ENR) & RCC_APB2ENR_SDIOEN) == 0U ||
        (NX_SDIO_READ(RCC->AHB1ENR) &
         (RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN)) !=
            (RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN))
        return NX_ERROR_STATE;
    return NX_SUCCESS;
}

static nx_result_t block_activate(void* context, nx_time_us_t deadline) {
    nx_stm32_sdio_state_t* state = context;
    nx_result_t result = operation_valid(state, deadline);
    if (result != NX_SUCCESS)
        return result;
    if (state->state != NX_BLOCK_STOPPED || state->protocol_started)
        return NX_ERROR_STATE;
    nx_block_state_t previous = state->state;
    state->state = NX_BLOCK_ACTIVE;
    if (nx_time_now_us() >= deadline) {
        state->state = previous;
        return NX_ERROR_TIMEOUT;
    }
    sdio_poll_t poll = {deadline, state->config->poll_budget, 0U};
    result = initialize_card(state, &poll);
    if (result == NX_SUCCESS) {
        state->geometry_valid = true;
        state->state = NX_BLOCK_READY;
    } else {
        state->geometry_valid = false;
        state->state = NX_BLOCK_UNCERTAIN;
    }
    return result;
}

static bool overlaps(uintptr_t start, size_t size, const void* other,
                     size_t other_size) {
    uintptr_t begin = (uintptr_t)other;
    return start < begin + other_size && begin < start + size;
}

static bool valid_buffer(const nx_stm32_sdio_state_t* state, const void* data,
                         size_t bytes) {
    uintptr_t start = (uintptr_t)data;
    if (data == NULL || (start & 3U) != 0U || bytes > UINTPTR_MAX - start ||
        overlaps(start, bytes, state, sizeof(*state)) ||
        overlaps(start, bytes, state->config, sizeof(*state->config)) ||
        overlaps(start, bytes, state->config->pins,
                 6U * sizeof(*state->config->pins)) ||
        overlaps(start, bytes, (void*)UINT32_C(0x40000000),
                 UINT32_C(0x20000000)) ||
        overlaps(start, bytes, (void*)UINT32_C(0xE0000000),
                 UINT32_C(0x10000000)))
        return false;
    for (size_t i = 0U; i < 6U; ++i)
        if (overlaps(start, bytes, state->config->pins[i].snapshot,
                     sizeof(uint16_t)))
            return false;
    return true;
}

static nx_block_io_result_t
block_transfer(void* context, uint64_t first, void* data, size_t bytes,
               uint32_t blocks, nx_time_us_t deadline, bool reading) {
    nx_stm32_sdio_state_t* state = context;
    nx_block_io_result_t outcome = {operation_valid(state, deadline), 0U};
    if (outcome.result != NX_SUCCESS)
        return outcome;
    if (state->state != NX_BLOCK_READY || !state->geometry_valid) {
        outcome.result = NX_ERROR_STATE;
        return outcome;
    }
    size_t needed = (size_t)blocks * 512U;
    if (blocks == 0U || blocks > state->config->max_blocks || bytes < needed ||
        !valid_buffer(state, data, needed) ||
        first >= state->geometry.block_count ||
        blocks > state->geometry.block_count - first ||
        (state->high_capacity && first + blocks - 1U > UINT32_MAX) ||
        (!state->high_capacity && first + blocks - 1U > UINT32_MAX / 512U)) {
        outcome.result = NX_ERROR_INVALID;
        return outcome;
    }
    if (!reading && !state->geometry.writable) {
        outcome.result = NX_ERROR_PERMISSION;
        return outcome;
    }
    state->state = NX_BLOCK_ACTIVE;
    if (nx_time_now_us() >= deadline) {
        state->state = NX_BLOCK_READY;
        outcome.result = NX_ERROR_TIMEOUT;
        return outcome;
    }
    sdio_poll_t poll = {deadline, state->config->poll_budget, 0U};
    for (uint32_t i = 0U; i < blocks; ++i) {
        uint64_t sector = first + i;
        uint32_t argument =
            (uint32_t)(state->high_capacity ? sector : sector * 512U);
        if (reading) {
            outcome.result = data_configure(512U, true, &poll);
            if (outcome.result == NX_SUCCESS)
                outcome.result =
                    command(17U, argument, RESPONSE_R1, &poll, NULL);
        } else {
            state->programming_pending = true;
            outcome.result = command(24U, argument, RESPONSE_R1, &poll, NULL);
            if (outcome.result == NX_SUCCESS)
                outcome.result = data_configure(512U, false, &poll);
        }
        if (outcome.result == NX_SUCCESS)
            outcome.result = fifo_transfer((uint8_t*)data + (size_t)i * 512U,
                                           128U, reading, &poll);
        if (outcome.result == NX_SUCCESS && !reading)
            outcome.result = card_ready(state, &poll, false);
        if (outcome.result != NX_SUCCESS)
            break;
        ++outcome.completed_blocks;
    }
    if (outcome.result != NX_SUCCESS) {
        /* CPU buffer references end regardless of host/card readiness. There
         * is no DMA descriptor, ISR publisher or retained pointer to caller. */
        (void)host_withdraw(&poll);
        state->state = NX_BLOCK_UNCERTAIN;
    } else {
        state->state = NX_BLOCK_READY;
    }
    return outcome;
}

static nx_block_io_result_t block_read(void* context, uint64_t first,
                                       void* data, size_t bytes,
                                       uint32_t blocks, nx_time_us_t deadline) {
    return block_transfer(context, first, data, bytes, blocks, deadline, true);
}

static nx_block_io_result_t block_write(void* context, uint64_t first,
                                        const void* data, size_t bytes,
                                        uint32_t blocks,
                                        nx_time_us_t deadline) {
    return block_transfer(context, first, (void*)data, bytes, blocks, deadline,
                          false);
}

static nx_result_t block_sync(void* context, nx_time_us_t deadline) {
    nx_stm32_sdio_state_t* state = context;
    nx_result_t result = operation_valid(state, deadline);
    if (result != NX_SUCCESS)
        return result;
    if (!state->geometry_valid || !state->selected)
        return NX_ERROR_STATE;
    nx_block_state_t previous = state->state;
    state->state = NX_BLOCK_ACTIVE;
    if (nx_time_now_us() >= deadline) {
        state->state = previous;
        return NX_ERROR_TIMEOUT;
    }
    sdio_poll_t poll = {deadline, state->config->poll_budget, 0U};
    result = host_withdraw(&poll);
    if (result == NX_SUCCESS)
        result = card_ready(state, &poll, false);
    state->state = result == NX_SUCCESS ? NX_BLOCK_READY : NX_BLOCK_UNCERTAIN;
    return result;
}

static nx_result_t block_stop(void* context, nx_time_us_t deadline) {
    nx_stm32_sdio_state_t* state = context;
    nx_result_t result = operation_valid(state, deadline);
    if (result != NX_SUCCESS)
        return result;
    if (!state->protocol_started)
        return NX_SUCCESS;
    nx_block_state_t previous = state->state;
    state->state = NX_BLOCK_ACTIVE;
    if (nx_time_now_us() >= deadline) {
        state->state = previous;
        return NX_ERROR_TIMEOUT;
    }
    sdio_poll_t poll = {deadline, state->config->poll_budget, 0U};
    result = host_withdraw(&poll);
    if (result == NX_SUCCESS && state->selected)
        result = card_ready(state, &poll, true);
    if (result == NX_SUCCESS && state->selected) {
        result = command(7U, 0U, RESPONSE_NONE, &poll, NULL);
        if (result == NX_SUCCESS)
            state->selected = false;
    }
    if (result == NX_SUCCESS)
        result = control_settled(&poll);
    if (result == NX_SUCCESS) {
        NX_SDIO_WRITE(SDIO->CMD, 0U);
        NX_SDIO_WRITE(SDIO->CLKCR, 0U);
        result = control_settled(&poll);
        if (result == NX_SUCCESS)
            result =
                SDIO_PowerState_OFF(SDIO) == HAL_OK ? NX_SUCCESS : NX_ERROR_IO;
        nx_arch_dsb();
        if (NX_SDIO_READ(SDIO->POWER) != 0U ||
            NX_SDIO_READ(SDIO->CLKCR) != 0U || !host_idle())
            result = NX_ERROR_IO;
    }
    if (result == NX_SUCCESS) {
        state->geometry_valid = false;
        state->protocol_started = false;
        state->selected = false;
        state->programming_pending = false;
        state->rca = 0U;
        state->state = NX_BLOCK_STOPPED;
    } else {
        state->state = NX_BLOCK_UNCERTAIN;
    }
    return result;
}

const nx_block_ops_t nx_stm32_sdio_ops = {
    .geometry = block_geometry,
    .state = block_state,
    .activate = block_activate,
    .read = block_read,
    .write = block_write,
    .sync = block_sync,
    .stop = block_stop,
};

static bool cold_disjoint(const nx_stm32_sdio_state_t* state,
                          const nx_stm32_sdio_config_t* config,
                          const nx_stm32_sdio_receipt_t* receipt) {
    uintptr_t start = (uintptr_t)state;
    uintptr_t acquired = (uintptr_t)receipt;
    if (overlaps(start, sizeof(*state), receipt, sizeof(*receipt)) ||
        overlaps(start, sizeof(*state), config, sizeof(*config)) ||
        overlaps(acquired, sizeof(*receipt), config, sizeof(*config)) ||
        overlaps(start, sizeof(*state), config->pins,
                 6U * sizeof(*config->pins)) ||
        overlaps(acquired, sizeof(*receipt), config->pins,
                 6U * sizeof(*config->pins)))
        return false;
    for (size_t i = 0U; i < 6U; ++i) {
        uintptr_t snapshot = (uintptr_t)config->pins[i].snapshot;
        if (overlaps(start, sizeof(*state), (void*)snapshot,
                     sizeof(uint16_t)) ||
            overlaps(acquired, sizeof(*receipt), (void*)snapshot,
                     sizeof(uint16_t)) ||
            overlaps(snapshot, sizeof(uint16_t), config, sizeof(*config)) ||
            overlaps(snapshot, sizeof(uint16_t), config->pins,
                     6U * sizeof(*config->pins)))
            return false;
    }
    return true;
}

nx_result_t nx_stm32_sdio_prepare(nx_stm32_sdio_state_t* state,
                                  const nx_stm32_sdio_config_t* config,
                                  nx_stm32_sdio_receipt_t* receipt) {
    if (!allowed_context())
        return NX_ERROR_CONTEXT;
    if (!cpu_storage(state, sizeof(*state), _Alignof(nx_stm32_sdio_state_t)) ||
        !cpu_storage(receipt, sizeof(*receipt),
                     _Alignof(nx_stm32_sdio_receipt_t)) ||
        !valid_config(config) || !cold_disjoint(state, config, receipt))
        return NX_ERROR_INVALID;
    if (state->infrastructure || state->config != NULL || receipt->owned ||
        receipt->pin_mask != 0U || receipt->clock_added != 0U)
        return NX_ERROR_BUSY;
    if (!live_clock() || (NX_SDIO_READ(RCC->AHB1ENR) &
                          (RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN)) !=
                             (RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN))
        return NX_ERROR_STATE;
    uint32_t bank = (uint32_t)SDIO_IRQn / 32U;
    uint32_t irq_mask = UINT32_C(1) << ((uint32_t)SDIO_IRQn % 32U);
    if (((NVIC->ISER[bank] | NVIC->ISPR[bank] | NVIC->IABR[bank]) & irq_mask) !=
        0U)
        return NX_ERROR_BUSY;
    if ((NX_SDIO_READ(RCC->APB2ENR) & RCC_APB2ENR_SDIOEN) != 0U &&
        (NX_SDIO_READ(SDIO->POWER) != 0U ||
         (NX_SDIO_READ(SDIO->CLKCR) & SDIO_CLKCR_CLKEN) != 0U ||
         NX_SDIO_READ(SDIO->MASK) != 0U || NX_SDIO_READ(SDIO->DCTRL) != 0U ||
         !host_idle()))
        return NX_ERROR_BUSY;
    const nx_stm32_platform_config_t pads = {.pins = config->pins,
                                             .pin_count = 6U};
    nx_result_t result = nx_stm32_platform_pins_admit(&pads, false);
    if (result != NX_SUCCESS)
        return result;
    receipt->owned = true;
    receipt->clock_added = ~NX_SDIO_READ(RCC->APB2ENR) & RCC_APB2ENR_SDIOEN;
    state->config = config;
    NX_SDIO_WRITE(RCC->APB2ENR,
                  NX_SDIO_READ(RCC->APB2ENR) | RCC_APB2ENR_SDIOEN);
    nx_arch_dsb();
    if ((NX_SDIO_READ(RCC->APB2ENR) & RCC_APB2ENR_SDIOEN) == 0U)
        return NX_ERROR_IO;
    for (size_t i = 0U; i < 6U; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        *pin->snapshot = nx_stm32_pin_capture(pin->registers, pin->pin);
        receipt->pin_mask |= (uint8_t)(1U << i);
        result = nx_stm32_pin_configure(pin->registers, pin->pin, 2U, 12U,
                                        (uint8_t)(i == 4U ? 0U : 1U), false);
        if (result != NX_SUCCESS)
            return result;
        uint32_t shift = 2U * pin->pin;
        uint32_t pull = i == 4U ? 0U : 1U;
        nx_arch_dsb();
        if (((NX_SDIO_READ(pin->registers->MODER) >> shift) & 3U) != 2U ||
            ((NX_SDIO_READ(pin->registers->AFR[pin->pin / 8U]) >>
              (4U * (pin->pin % 8U))) &
             15U) != 12U ||
            ((NX_SDIO_READ(pin->registers->PUPDR) >> shift) & 3U) != pull ||
            ((NX_SDIO_READ(pin->registers->OTYPER) >> pin->pin) & 1U) != 0U ||
            ((NX_SDIO_READ(pin->registers->OSPEEDR) >> shift) & 3U) != 2U)
            return NX_ERROR_IO;
    }
    state->infrastructure = true;
    state->state = NX_BLOCK_STOPPED;
    return NX_SUCCESS;
}

nx_result_t nx_stm32_sdio_close(nx_stm32_sdio_state_t* state,
                                const nx_stm32_sdio_config_t* config,
                                nx_stm32_sdio_receipt_t* receipt) {
    if (!allowed_context())
        return NX_ERROR_CONTEXT;
    if (!cpu_storage(state, sizeof(*state), _Alignof(nx_stm32_sdio_state_t)) ||
        !cpu_storage(receipt, sizeof(*receipt),
                     _Alignof(nx_stm32_sdio_receipt_t)) ||
        !valid_config(config) || !cold_disjoint(state, config, receipt))
        return NX_ERROR_INVALID;
    if (!receipt->owned &&
        (receipt->pin_mask != 0U || receipt->clock_added != 0U))
        return NX_ERROR_INVALID;
    if (!receipt->owned)
        return state->config != NULL || state->infrastructure ? NX_ERROR_BUSY
                                                              : NX_SUCCESS;
    if (state->config != config || (receipt->pin_mask & ~UINT8_C(63)) != 0U ||
        (receipt->clock_added & ~RCC_APB2ENR_SDIOEN) != 0U)
        return NX_ERROR_INVALID;
    if (state->state == NX_BLOCK_ACTIVE)
        return NX_ERROR_BUSY;
    if (state->protocol_started) {
        nx_time_us_t deadline;
        nx_block_state_t previous = state->state;
        state->state = NX_BLOCK_ACTIVE;
        nx_result_t result = nx_deadline_finite_after(
            nx_time_now_us(), config->stop_timeout_us, &deadline);
        state->state = previous;
        if (result != NX_SUCCESS)
            return result;
        result = block_stop(state, deadline);
        if (result != NX_SUCCESS)
            return result;
    }
    if (!host_idle())
        return NX_ERROR_BUSY;
    for (size_t i = 6U; i != 0U; --i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i - 1U];
        uint8_t mask = (uint8_t)(1U << (i - 1U));
        if ((receipt->pin_mask & mask) == 0U)
            continue;
        nx_stm32_pin_restore(pin->registers, pin->pin, *pin->snapshot);
        nx_arch_dsb();
        if (nx_stm32_pin_capture(pin->registers, pin->pin) != *pin->snapshot)
            return NX_ERROR_IO;
        receipt->pin_mask &= (uint8_t)~mask;
    }
    NX_SDIO_WRITE(RCC->APB2ENR,
                  NX_SDIO_READ(RCC->APB2ENR) & ~receipt->clock_added);
    nx_arch_dsb();
    if ((NX_SDIO_READ(RCC->APB2ENR) & receipt->clock_added) != 0U)
        return NX_ERROR_IO;
    receipt->clock_added = 0U;
    receipt->owned = false;
    *state = (nx_stm32_sdio_state_t){0};
    return NX_SUCCESS;
}
