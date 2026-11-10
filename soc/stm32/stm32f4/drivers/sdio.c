/**
 * \file            sdio.c
 * \brief           Exact SD/MMC polling and finite IRQ/DMA block ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
#include "stm32f407_sdio.h"
#include "stm32f4xx_ll_dma.h"
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

/** \brief Wire width selects an exact static pad/snapshot extent. */
static uint8_t pin_count(const nx_stm32_sdio_config_t* config) {
    return config->bus_width == 8U ? 10U : 6U;
}

static bool valid_config(const nx_stm32_sdio_config_t* config) {
    if (!cpu_storage(config, sizeof(*config),
                     _Alignof(nx_stm32_sdio_config_t)) ||
        config->registers != SDIO ||
        !cpu_storage(config->pins, pin_count(config) * sizeof(*config->pins),
                     _Alignof(nx_stm32_platform_pin_config_t)) ||
        (config->bus_width != 1U && config->bus_width != 4U &&
         config->bus_width != 8U) ||
        (config->bus_width == 8U && config->protocol != NX_STM32_SDIO_MMC) ||
        config->max_blocks == 0U || config->max_blocks > 32U ||
        config->poll_budget == 0U || config->poll_budget > 1000000U ||
        config->stop_timeout_us == 0U ||
        (config->engine != NX_STM32_SDIO_POLL &&
         config->engine != NX_STM32_SDIO_IRQ &&
         config->engine != NX_STM32_SDIO_DMA) ||
        (config->protocol != NX_STM32_SDIO_SD_MEMORY &&
         config->protocol != NX_STM32_SDIO_MMC &&
         config->protocol != NX_STM32_SDIO_IO) ||
        ((config->engine != NX_STM32_SDIO_POLL ||
          config->protocol == NX_STM32_SDIO_IO) &&
         (!cpu_storage(config->async, sizeof(*config->async),
                       _Alignof(nx_stm32_sdio_async_state_t)) ||
          config->irq_priority > 15U)) ||
        (config->engine == NX_STM32_SDIO_POLL &&
         config->protocol != NX_STM32_SDIO_IO && config->async != NULL) ||
        (config->protocol == NX_STM32_SDIO_IO
             ? !cpu_storage(config->io, sizeof(*config->io),
                            _Alignof(nx_stm32_sdio_io_state_t))
             : config->io != NULL) ||
        (config->engine == NX_STM32_SDIO_DMA &&
         (config->dma_region_count == 0U || config->dma_region_count > 8U ||
          !cpu_storage(config->dma_regions,
                       config->dma_region_count * sizeof(*config->dma_regions),
                       _Alignof(nx_dma_memory_region_t)))))
        return false;
    for (size_t i = 0U; i < pin_count(config); ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        GPIO_TypeDef* bank = i == 5U              ? GPIOD
                             : i == 6U || i == 7U ? GPIOB
                                                  : GPIOC;
        uint8_t port = bank == GPIOD ? 3U : bank == GPIOB ? 1U : 2U;
        size_t number = i == 5U  ? 2U
                        : i < 5U ? 8U + i
                        : i < 8U ? i + 2U
                                 : i - 2U;
        if (pin->registers != bank || pin->port_index != port ||
            pin->pin != number ||
            !cpu_storage(pin->snapshot, sizeof(uint16_t), _Alignof(uint16_t)))
            return false;
        for (size_t j = 0U; j < i; ++j)
            if (pin->snapshot == config->pins[j].snapshot)
                return false;
    }
    return true;
}

static bool live_clock(void) {
    return NEXUS_PLL48_HZ == 48000000U &&
           nx_stm32_clock_profile_matches(RCC, NULL);
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
    RESPONSE_R4,
    RESPONSE_R5,
    RESPONSE_R6,
    RESPONSE_R7
} sdio_response_t;

#define COMMAND_FLAGS                                                          \
    (SDIO_STA_CCRCFAIL | SDIO_STA_CTIMEOUT | SDIO_STA_CMDREND |                \
     SDIO_STA_CMDSENT)
#define DATA_ERRORS                                                            \
    (SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_TXUNDERR |               \
     SDIO_STA_RXOVERR | SDIO_STA_STBITERR)
#define DATA_FLAGS       (DATA_ERRORS | SDIO_STA_DATAEND | SDIO_STA_DBCKEND)
#define CARD_ERRORS      UINT32_C(0xFFFFE008)
#define MMC_SWITCH_ERROR (UINT32_C(1) << 7U)

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
    if ((status & SDIO_STA_CCRCFAIL) != 0U && response != RESPONSE_R3 &&
        response != RESPONSE_R4)
        return NX_ERROR_IO;
    if (response != RESPONSE_NONE && response != RESPONSE_R2 &&
        response != RESPONSE_R3 && response != RESPONSE_R4 &&
        (NX_SDIO_READ(SDIO->RESPCMD) & 63U) != index)
        return NX_ERROR_IO;
    if (value != NULL)
        *value = reply;
    if (response == RESPONSE_R1 && (reply & CARD_ERRORS) != 0U)
        return NX_ERROR_IO;
    if (response == RESPONSE_R6 && (reply & UINT32_C(0xE000)) != 0U)
        return NX_ERROR_IO;
    if (response == RESPONSE_R5 && (reply & UINT32_C(0xcb00)) != 0U)
        return NX_ERROR_IO;
    return NX_SUCCESS;
}

static nx_result_t io_register(uint8_t function, uint32_t address,
                               uint8_t* value, bool write, bool readback,
                               sdio_poll_t* poll) {
    uint32_t argument = ((uint32_t)function << 28U) | (address << 9U);
    if (write)
        argument |= (UINT32_C(1) << 31U) | *value;
    if (readback)
        argument |= UINT32_C(1) << 27U;
    uint32_t reply = 0U;
    nx_result_t result = command(52U, argument, RESPONSE_R5, poll, &reply);
    if (result == NX_SUCCESS && (!write || readback))
        *value = (uint8_t)reply;
    return result;
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

static nx_result_t host_withdraw(sdio_poll_t* poll, uint32_t* flags) {
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
            if (flags != NULL)
                *flags |= NX_SDIO_READ(SDIO->STA) & DATA_FLAGS;
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
        if (state->config->protocol == NX_STM32_SDIO_MMC &&
            (status & MMC_SWITCH_ERROR) != 0U)
            return NX_ERROR_IO;
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

static nx_result_t data_configure(const nx_stm32_sdio_state_t* state,
                                  uint32_t bytes, bool reading,
                                  sdio_poll_t* poll, bool dma, bool byte_mode) {
    if ((NX_SDIO_READ(SDIO->DCTRL) & SDIO_DCTRL_DTEN) != 0U || !host_idle())
        return NX_ERROR_BUSY;
    nx_result_t result = control_settled(poll);
    if (result != NX_SUCCESS)
        return result;
    NX_SDIO_WRITE(SDIO->ICR, DATA_FLAGS);
    uint32_t block_bits = 0U;
    while ((UINT32_C(1) << block_bits) < bytes && block_bits < 11U)
        ++block_bits;
    bool io = state->config->protocol == NX_STM32_SDIO_IO;
    const SDIO_DataInitTypeDef data = {
        .DataTimeOut = UINT32_C(0x00FFFFFF),
        .DataLength = bytes,
        .DataBlockSize = byte_mode ? 0U : block_bits << 4U,
        .TransferDir =
            reading ? SDIO_TRANSFER_DIR_TO_SDIO : SDIO_TRANSFER_DIR_TO_CARD,
        .TransferMode =
            byte_mode ? SDIO_TRANSFER_MODE_STREAM : SDIO_TRANSFER_MODE_BLOCK,
        .DPSM = dma || io ? SDIO_DPSM_DISABLE : SDIO_DPSM_ENABLE,
    };
    SDIO_DataInitTypeDef selected = data;
    if (SDIO_ConfigData(SDIO, &selected) != HAL_OK)
        return NX_ERROR_IO;
    if (dma || io) {
        result = control_settled(poll);
        if (result != NX_SUCCESS)
            return result;
        uint32_t control = NX_SDIO_READ(SDIO->DCTRL) | SDIO_DCTRL_DTEN |
                           (dma ? SDIO_DCTRL_DMAEN : 0U) |
                           (io ? SDIO_DCTRL_SDIOEN : 0U);
        NX_SDIO_WRITE(SDIO->DCTRL, control);
        nx_arch_dsb();
        if (NX_SDIO_READ(SDIO->DCTRL) != control)
            return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

static nx_result_t fifo_transfer(void* data, uint32_t bytes, bool reading,
                                 sdio_poll_t* poll) {
    uint32_t words = (bytes + 3U) / 4U;
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
            size_t count = bytes - copied * 4U;
            if (count > sizeof(word))
                count = sizeof(word);
            memcpy(buffer + (size_t)copied * sizeof(word), &word, count);
            ++copied;
        } else if (!reading && (status & SDIO_STA_TXFIFOHE) != 0U &&
                   copied < words) {
            uint32_t word = 0U;
            size_t count = bytes - copied * 4U;
            if (count > sizeof(word))
                count = sizeof(word);
            memcpy(&word, buffer + (size_t)copied * sizeof(word), count);
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
    } else if (!state->high_capacity &&
               (structure == 0U ||
                (state->config->protocol == NX_STM32_SDIO_MMC &&
                 structure <= 2U))) {
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
        .buffer_alignment =
            state->config->engine == NX_STM32_SDIO_DMA ? 16U : 4U,
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

static uint8_t io_function_mask(const nx_stm32_sdio_state_t* state) {
    return (uint8_t)((UINT32_C(1) << (state->config->io->function_count + 1U)) -
                     2U);
}

static nx_result_t io_program(uint32_t address, uint8_t value,
                              sdio_poll_t* poll) {
    uint8_t observed = value;
    nx_result_t result = io_register(0U, address, &observed, true, true, poll);
    return result != NX_SUCCESS ? result
           : observed == value  ? NX_SUCCESS
                                : NX_ERROR_IO;
}

static nx_result_t io_wait_ready(nx_stm32_sdio_state_t* state, uint8_t enabled,
                                 sdio_poll_t* poll) {
    for (;;) {
        uint8_t ready = 0U;
        nx_result_t result = io_register(0U, 3U, &ready, false, false, poll);
        if (result != NX_SUCCESS)
            return result;
        uint8_t observed =
            ready & (enabled == 0U ? UINT8_C(0xfe) : io_function_mask(state));
        if (observed == enabled)
            return NX_SUCCESS;
    }
}

static nx_result_t initialize_io(nx_stm32_sdio_state_t* state,
                                 sdio_poll_t* poll) {
    nx_stm32_sdio_io_state_t* io = state->config->io;
    *io = (nx_stm32_sdio_io_state_t){0};
    uint32_t ocr = 0U;
    /* CMD0 alone does not reset the I/O portion. Enable CMD52 using CMD5,
     * explicitly reset I/O, then repeat voltage/readiness negotiation. */
    for (unsigned pass = 0U; pass < 2U; ++pass) {
        do {
            nx_result_t result =
                command(5U, UINT32_C(0x00ff8000), RESPONSE_R4, poll, &ocr);
            if (result != NX_SUCCESS)
                return result;
        } while ((ocr & (UINT32_C(1) << 31U)) == 0U);
        io->function_count = (uint8_t)((ocr >> 28U) & 7U);
        io->memory_present = (ocr & (UINT32_C(1) << 27U)) != 0U;
        if ((ocr & UINT32_C(0x00ff8000)) == 0U || io->function_count == 0U)
            return NX_ERROR_UNSUPPORTED;
        if (pass == 0U) {
            uint8_t reset = 8U;
            nx_result_t result = io_register(0U, 6U, &reset, true, false, poll);
            if (result != NX_SUCCESS)
                return result;
        }
    }
    uint32_t rca = 0U;
    nx_result_t result = command(3U, 0U, RESPONSE_R6, poll, &rca);
    if (result != NX_SUCCESS)
        return result;
    state->rca = (uint16_t)(rca >> 16U);
    if (state->rca == 0U)
        return NX_ERROR_IO;
    state->selected = true;
    result = command(7U, (uint32_t)state->rca << 16U, RESPONSE_R1, poll, NULL);
    if (result != NX_SUCCESS)
        return result;
    uint8_t capabilities = 0U;
    result = io_register(0U, 8U, &capabilities, false, false, poll);
    if (result != NX_SUCCESS)
        return result;
    bool low_speed = (capabilities & 64U) != 0U;
    if (state->config->bus_width == 4U) {
        if (low_speed && (capabilities & 128U) == 0U)
            return NX_ERROR_UNSUPPORTED;
        uint8_t control = 0U;
        result = io_register(0U, 7U, &control, false, false, poll);
        if (result == NX_SUCCESS)
            result = io_program(7U, (uint8_t)((control & ~3U) | 2U), poll);
        if (result != NX_SUCCESS)
            return result;
    }
    return configured_clock(low_speed ? 118U : 2U,
                            state->config->bus_width == 4U ? SDIO_BUS_WIDE_4B
                                                           : SDIO_BUS_WIDE_1B,
                            poll);
}

static nx_result_t io_shutdown(nx_stm32_sdio_state_t* state,
                               sdio_poll_t* poll) {
    nx_stm32_sdio_io_state_t* io = state->config->io;
    io->interrupt_armed = false;
    nx_result_t result = NX_SUCCESS;
    if (io->inflight != 0U) {
        uint8_t abort = io->inflight;
        result = io_register(0U, 6U, &abort, true, false, poll);
        if (result == NX_SUCCESS)
            io->inflight = 0U;
    }
    if (result == NX_SUCCESS)
        result = io_program(4U, 0U, poll);
    if (result == NX_SUCCESS)
        result = io_program(2U, 0U, poll);
    if (result == NX_SUCCESS)
        result = io_wait_ready(state, 0U, poll);
    if (result == NX_SUCCESS) {
        io->enabled = 0U;
        io->interrupts = 0U;
        io->interrupt_latched = false;
    }
    return result;
}

static nx_result_t initialize_mmc(nx_stm32_sdio_state_t* state,
                                  sdio_poll_t* poll) {
    uint32_t ocr = 0U;
    nx_result_t result;
    do {
        result = command(1U, UINT32_C(0x40ff8000), RESPONSE_R3, poll, &ocr);
        if (result != NX_SUCCESS)
            return result;
    } while ((ocr & (UINT32_C(1) << 31U)) == 0U);
    if ((ocr & UINT32_C(0x00ff8000)) == 0U)
        return NX_ERROR_UNSUPPORTED;
    state->high_capacity = (ocr & (UINT32_C(1) << 30U)) != 0U;
    result = command(2U, 0U, RESPONSE_R2, poll, NULL);
    if (result != NX_SUCCESS)
        return result;
    state->cid[0] = NX_SDIO_READ(SDIO->RESP1);
    state->cid[1] = NX_SDIO_READ(SDIO->RESP2);
    state->cid[2] = NX_SDIO_READ(SDIO->RESP3);
    state->cid[3] = NX_SDIO_READ(SDIO->RESP4);
    /* MMC assigns RCA from the host; SD SEND_RELATIVE_ADDR instead returns
     * an RCA in R6. These are distinct protocols, not a fallback probe. */
    state->rca = 1U;
    result = command(3U, UINT32_C(1) << 16U, RESPONSE_R1, poll, NULL);
    if (result != NX_SUCCESS)
        return result;
    result = command(9U, UINT32_C(1) << 16U, RESPONSE_R2, poll, NULL);
    if (result != NX_SUCCESS)
        return result;
    uint8_t csd[16];
    response_bytes(csd);
    /* MMC v4 introduces the extended bus-width field. Reserved CSD version
     * values are not permission to send an unsupported SWITCH command. */
    if (state->config->bus_width == 8U && ((csd[0] >> 2U) & 15U) != 4U)
        return NX_ERROR_UNSUPPORTED;
    if (!state->high_capacity) {
        result = decode_csd(state);
        if (result != NX_SUCCESS)
            return result;
    }
    state->selected = true;
    result = command(7U, UINT32_C(1) << 16U, RESPONSE_R1, poll, NULL);
    if (result == NX_SUCCESS)
        result = card_ready(state, poll, false);
    if (result != NX_SUCCESS)
        return result;
    if (state->high_capacity) {
        /* EXT_CSD is one fixed 512-byte transfer in the identification bus
         * width. No heap, hidden DMA bounce buffer or stale geometry cache. */
        uint32_t extended[128];
        result = data_configure(state, 512U, true, poll, false, false);
        if (result == NX_SUCCESS)
            result = command(8U, 0U, RESPONSE_R1, poll, NULL);
        if (result == NX_SUCCESS)
            result = fifo_transfer(extended, 512U, true, poll);
        if (result != NX_SUCCESS)
            return result;
        const uint8_t* bytes = (const uint8_t*)extended;
        uint32_t sectors = (uint32_t)bytes[212] | ((uint32_t)bytes[213] << 8U) |
                           ((uint32_t)bytes[214] << 16U) |
                           ((uint32_t)bytes[215] << 24U);
        if (bytes[192] < 2U || sectors == 0U)
            return NX_ERROR_UNSUPPORTED;
        state->geometry = (nx_block_geometry_t){
            .block_count = sectors,
            .block_bytes = 512U,
            .buffer_alignment =
                state->config->engine == NX_STM32_SDIO_DMA ? 16U : 4U,
            .writable = (csd[14] & 48U) == 0U,
        };
    } else {
        result = command(16U, 512U, RESPONSE_R1, poll, NULL);
        if (result != NX_SUCCESS)
            return result;
    }
    if (state->config->bus_width != 1U) {
        uint32_t width = state->config->bus_width == 8U ? 2U : 1U;
        uint32_t status = 0U;
        result = command(6U, UINT32_C(0x03b70000) | (width << 8U), RESPONSE_R1,
                         poll, &status);
        if (result == NX_SUCCESS && (status & MMC_SWITCH_ERROR) != 0U)
            result = NX_ERROR_IO;
        if (result == NX_SUCCESS)
            result = card_ready(state, poll, false);
        if (result != NX_SUCCESS)
            return result;
    }
    return configured_clock(2U,
                            state->config->bus_width == 8U   ? SDIO_BUS_WIDE_8B
                            : state->config->bus_width == 4U ? SDIO_BUS_WIDE_4B
                                                             : SDIO_BUS_WIDE_1B,
                            poll);
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
    if (state->config->protocol == NX_STM32_SDIO_MMC)
        return initialize_mmc(state, poll);
    if (state->config->protocol == NX_STM32_SDIO_IO)
        return initialize_io(state, poll);
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
        result = data_configure(state, 8U, true, poll, false, false);
        if (result == NX_SUCCESS)
            result = app_command(state, 51U, 0U, RESPONSE_R1, poll, NULL);
        if (result == NX_SUCCESS)
            result = fifo_transfer(scr, 8U, true, poll);
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
    state->removed = false;
    nx_block_state_t previous = state->state;
    state->state = NX_BLOCK_ACTIVE;
    if (nx_time_now_us() >= deadline) {
        state->state = previous;
        return NX_ERROR_TIMEOUT;
    }
    sdio_poll_t poll = {deadline, state->config->poll_budget, 0U};
    result = initialize_card(state, &poll);
    if (result == NX_SUCCESS) {
        state->geometry_valid = state->config->protocol != NX_STM32_SDIO_IO;
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

/* A valid CPU span may be immutable Flash (for TX). It cannot be a receive
 * destination or mutable request. CPU-only CCM is not ROM. */
static bool cpu_write_span(const void* pointer, size_t bytes) {
    uintptr_t start = (uintptr_t)pointer;
    return !overlaps(start, bytes, (void*)(uintptr_t)UINT32_C(0x08000000),
                     UINT32_C(0x00100000)) &&
           !overlaps(start, bytes, (void*)(uintptr_t)UINT32_C(0x1fff0000),
                     UINT32_C(0x10000));
}

static bool buffer_at(const nx_stm32_sdio_state_t* state, const void* data,
                      size_t bytes, size_t alignment) {
    uintptr_t start = (uintptr_t)data;
    if (data == NULL || start % alignment != 0U ||
        bytes > UINTPTR_MAX - start ||
        overlaps(start, bytes, state, sizeof(*state)) ||
        overlaps(start, bytes, state->config, sizeof(*state->config)) ||
        overlaps(start, bytes, state->config->pins,
                 pin_count(state->config) * sizeof(*state->config->pins)) ||
        overlaps(start, bytes, (void*)UINT32_C(0x40000000),
                 UINT32_C(0x20000000)) ||
        overlaps(start, bytes, (void*)UINT32_C(0xE0000000),
                 UINT32_C(0x10000000)) ||
        (state->config->async != NULL &&
         overlaps(start, bytes, state->config->async,
                  sizeof(*state->config->async))) ||
        (state->config->io != NULL && overlaps(start, bytes, state->config->io,
                                               sizeof(*state->config->io))) ||
        (state->config->dma_region_count != 0U &&
         overlaps(start, bytes, state->config->dma_regions,
                  state->config->dma_region_count *
                      sizeof(*state->config->dma_regions))))
        return false;
    for (size_t i = 0U; i < pin_count(state->config); ++i)
        if (overlaps(start, bytes, state->config->pins[i].snapshot,
                     sizeof(uint16_t)))
            return false;
    return true;
}

static bool valid_buffer(const nx_stm32_sdio_state_t* state, const void* data,
                         size_t bytes) {
    return buffer_at(state, data, bytes, 4U);
}

static nx_block_io_result_t
block_transfer(void* context, uint64_t first, void* data, size_t bytes,
               uint32_t blocks, nx_time_us_t deadline, bool reading) {
    nx_stm32_sdio_state_t* state = context;
    nx_block_io_result_t outcome = {operation_valid(state, deadline), 0U};
    if (outcome.result != NX_SUCCESS)
        return outcome;
    if (state->config->protocol == NX_STM32_SDIO_IO) {
        outcome.result = NX_ERROR_UNSUPPORTED;
        return outcome;
    }
    if (state->config->engine != NX_STM32_SDIO_POLL) {
        outcome.result = NX_ERROR_UNSUPPORTED;
        return outcome;
    }
    if (state->state != NX_BLOCK_READY || !state->geometry_valid) {
        outcome.result = NX_ERROR_STATE;
        return outcome;
    }
    size_t needed = (size_t)blocks * 512U;
    if (blocks == 0U || blocks > state->config->max_blocks || bytes < needed ||
        !valid_buffer(state, data, needed) ||
        (reading && !cpu_write_span(data, needed)) ||
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
            outcome.result =
                data_configure(state, 512U, true, &poll, false, false);
            if (outcome.result == NX_SUCCESS)
                outcome.result =
                    command(17U, argument, RESPONSE_R1, &poll, NULL);
        } else {
            state->programming_pending = true;
            outcome.result = command(24U, argument, RESPONSE_R1, &poll, NULL);
            if (outcome.result == NX_SUCCESS)
                outcome.result =
                    data_configure(state, 512U, false, &poll, false, false);
        }
        if (outcome.result == NX_SUCCESS)
            outcome.result = fifo_transfer((uint8_t*)data + (size_t)i * 512U,
                                           512U, reading, &poll);
        if (outcome.result == NX_SUCCESS && !reading)
            outcome.result = card_ready(state, &poll, false);
        if (outcome.result != NX_SUCCESS)
            break;
        ++outcome.completed_blocks;
    }
    if (outcome.result != NX_SUCCESS) {
        /* CPU buffer references end regardless of host/card readiness. There
         * is no DMA descriptor, ISR publisher or retained pointer to caller. */
        (void)host_withdraw(&poll, NULL);
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
    NX_SDIO_WRITE(SDIO->MASK, 0U);
    result = host_withdraw(&poll, NULL);
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
    if (state->config->async != NULL && state->config->async->active != NULL)
        return NX_ERROR_BUSY;
    if (!state->protocol_started) {
        state->state = NX_BLOCK_STOPPED;
        return NX_SUCCESS;
    }
    nx_block_state_t previous = state->state;
    state->state = NX_BLOCK_ACTIVE;
    if (nx_time_now_us() >= deadline) {
        state->state = previous;
        return NX_ERROR_TIMEOUT;
    }
    sdio_poll_t poll = {deadline, state->config->poll_budget, 0U};
    NX_SDIO_WRITE(SDIO->MASK, 0U);
    result = host_withdraw(&poll, NULL);
    if (state->removed) {
        /* Board evidence establishes that this card no longer drives the
         * socket. Host withdrawal still must complete before pins can return.
         * No response from an absent card is fabricated as media readiness. */
        state->selected = false;
        state->programming_pending = false;
    }
    if (result == NX_SUCCESS && !state->removed &&
        state->config->protocol == NX_STM32_SDIO_IO)
        result = io_shutdown(state, &poll);
    if (result == NX_SUCCESS && state->selected &&
        state->config->protocol != NX_STM32_SDIO_IO)
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

static nx_result_t media_removed(void* context);

const nx_block_ops_t nx_stm32_sdio_ops = {
    .geometry = block_geometry,
    .state = block_state,
    .activate = block_activate,
    .read = block_read,
    .write = block_write,
    .sync = block_sync,
    .stop = block_stop,
    .media_removed = media_removed,
};

#define NX_SDIO_DMA_FLAGS  (UINT32_C(0x3d) << 22U)
#define NX_SDIO_DMA_ERRORS (UINT32_C(0x0d) << 22U)

static void async_drain(nx_stm32_sdio_state_t* state, nx_result_t result) {
    nx_stm32_sdio_async_state_t* execution = state->config->async;
    if (!execution->draining) {
        if (result != NX_SUCCESS && state->config->io != NULL)
            state->config->io->interrupt_armed = false;
        execution->terminal = result;
        execution->draining = true;
        (void)nx_request_transition(&execution->active->base,
                                    NX_REQUEST_DRAINING);
    }
}

static bool dma_detach(const nx_stm32_sdio_state_t* state) {
    if (state->config->engine != NX_STM32_SDIO_DMA)
        return true;
    LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_3);
    LL_DMA_DisableIT_HT(DMA2, LL_DMA_STREAM_3);
    LL_DMA_DisableIT_TC(DMA2, LL_DMA_STREAM_3);
    LL_DMA_DisableIT_TE(DMA2, LL_DMA_STREAM_3);
    LL_DMA_DisableIT_DME(DMA2, LL_DMA_STREAM_3);
    LL_DMA_DisableIT_FE(DMA2, LL_DMA_STREAM_3);
    nx_arch_dsb();
    if ((NX_SDIO_READ(DMA2_Stream3->CR) &
         (DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE |
          DMA_SxCR_DMEIE)) != 0U ||
        (NX_SDIO_READ(DMA2_Stream3->FCR) & DMA_SxFCR_FEIE) != 0U)
        return false;
    LL_DMA_SetMemoryAddress(DMA2, LL_DMA_STREAM_3, 0U);
    LL_DMA_SetPeriphAddress(DMA2, LL_DMA_STREAM_3, 0U);
    LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_3, 0U);
    nx_stm32_sdio_async_state_t* execution = state->config->async;
    if (execution->active != NULL &&
        (execution->draining || execution->dma_complete) &&
        (NX_SDIO_READ(DMA2->LISR) & NX_SDIO_DMA_ERRORS) != 0U)
        execution->flags |= SDIO_STA_RXOVERR;
    NX_SDIO_WRITE(DMA2->LIFCR, NX_SDIO_DMA_FLAGS);
    NVIC_ClearPendingIRQ(DMA2_Stream3_IRQn);
    nx_arch_dsb();
    return NX_SDIO_READ(DMA2_Stream3->M0AR) == 0U &&
           NX_SDIO_READ(DMA2_Stream3->PAR) == 0U &&
           (NX_SDIO_READ(DMA2->LISR) & NX_SDIO_DMA_FLAGS) == 0U &&
           NVIC_GetPendingIRQ(DMA2_Stream3_IRQn) == 0U &&
           NVIC_GetActive(DMA2_Stream3_IRQn) == 0U;
}

static nx_result_t async_start_block(nx_stm32_sdio_state_t* state) {
    nx_stm32_sdio_async_state_t* execution = state->config->async;
    nx_block_request_t* request = execution->active;
    uint8_t* data = (uint8_t*)request->data +
                    (size_t)execution->completed_blocks * execution->unit_bytes;
    execution->words = 0U;
    execution->flags = 0U;
    execution->dma_complete = false;
    if (state->config->engine == NX_STM32_SDIO_DMA) {
        if (!dma_detach(state))
            return NX_ERROR_IO;
        LL_DMA_SetChannelSelection(DMA2, LL_DMA_STREAM_3, LL_DMA_CHANNEL_4);
        LL_DMA_ConfigTransfer(
            DMA2, LL_DMA_STREAM_3,
            (request->write ? LL_DMA_DIRECTION_MEMORY_TO_PERIPH
                            : LL_DMA_DIRECTION_PERIPH_TO_MEMORY) |
                LL_DMA_MODE_NORMAL | LL_DMA_PERIPH_NOINCREMENT |
                LL_DMA_MEMORY_INCREMENT | LL_DMA_PDATAALIGN_WORD |
                LL_DMA_MDATAALIGN_WORD | LL_DMA_PRIORITY_HIGH |
                LL_DMA_MBURST_INC4 | LL_DMA_PBURST_INC4);
        /* The F4 LL has no PFCTRL setter. SDIO is the peripheral flow
         * controller; the reviewed four-word FIFO bursts are never circular. */
        NX_SDIO_WRITE(DMA2_Stream3->CR,
                      NX_SDIO_READ(DMA2_Stream3->CR) | DMA_SxCR_PFCTRL);
        LL_DMA_EnableFifoMode(DMA2, LL_DMA_STREAM_3);
        LL_DMA_SetFIFOThreshold(DMA2, LL_DMA_STREAM_3,
                                LL_DMA_FIFOTHRESHOLD_FULL);
        LL_DMA_SetPeriphAddress(DMA2, LL_DMA_STREAM_3,
                                (uint32_t)(uintptr_t)&SDIO->FIFO);
        LL_DMA_SetMemoryAddress(DMA2, LL_DMA_STREAM_3,
                                (uint32_t)(uintptr_t)data);
        LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_3, execution->unit_bytes / 4U);
        LL_DMA_EnableIT_TC(DMA2, LL_DMA_STREAM_3);
        LL_DMA_EnableIT_TE(DMA2, LL_DMA_STREAM_3);
        LL_DMA_EnableIT_DME(DMA2, LL_DMA_STREAM_3);
        LL_DMA_EnableIT_FE(DMA2, LL_DMA_STREAM_3);
        LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_3);
        nx_arch_dsb();
        if (LL_DMA_IsEnabledStream(DMA2, LL_DMA_STREAM_3) == 0U)
            return NX_ERROR_IO;
    }
    sdio_poll_t poll = {request->base.deadline, state->config->poll_budget, 0U};
    uint64_t sector = request->first_block + execution->completed_blocks;
    uint32_t argument =
        (uint32_t)(state->high_capacity ? sector : sector * 512U);
    nx_result_t result;
    if (execution->function != 0U) {
        uint32_t address = execution->function_address;
        if (execution->increment)
            address += execution->completed_blocks * execution->unit_bytes;
        argument = ((uint32_t)execution->function << 28U) | (address << 9U) |
                   (request->write ? UINT32_C(1) << 31U : 0U) |
                   (execution->block_mode ? UINT32_C(1) << 27U : 0U) |
                   (execution->increment ? UINT32_C(1) << 26U : 0U) |
                   (execution->block_mode
                        ? 1U
                        : (uint32_t)execution->unit_bytes & UINT32_C(511));
        state->config->io->inflight = execution->function;
        result = data_configure(
            state, execution->unit_bytes, !request->write, &poll,
            state->config->engine == NX_STM32_SDIO_DMA, !execution->block_mode);
        if (result == NX_SUCCESS)
            result = command(53U, argument, RESPONSE_R5, &poll, NULL);
    } else if (request->write) {
        state->programming_pending = true;
        result = command(24U, argument, RESPONSE_R1, &poll, NULL);
        if (result == NX_SUCCESS)
            result = data_configure(state, 512U, false, &poll,
                                    state->config->engine == NX_STM32_SDIO_DMA,
                                    false);
    } else {
        result =
            data_configure(state, 512U, true, &poll,
                           state->config->engine == NX_STM32_SDIO_DMA, false);
        if (result == NX_SUCCESS)
            result = command(17U, argument, RESPONSE_R1, &poll, NULL);
    }
    if (result != NX_SUCCESS)
        return result;
    uint32_t mask = state->config->engine == NX_STM32_SDIO_POLL
                        ? 0U
                        : DATA_ERRORS | SDIO_STA_DATAEND | SDIO_STA_DBCKEND;
    if (state->config->engine == NX_STM32_SDIO_IRQ)
        mask |= request->write ? SDIO_STA_TXFIFOHE : SDIO_STA_RXDAVL;
    if (state->config->engine != NX_STM32_SDIO_POLL &&
        state->config->io != NULL && state->config->io->interrupts != 0U &&
        state->config->io->interrupt_armed)
        mask |= SDIO_STA_SDIOIT;
    NX_SDIO_WRITE(SDIO->MASK, mask);
    nx_arch_dsb();
    return NX_SDIO_READ(SDIO->MASK) == mask ? NX_SUCCESS : NX_ERROR_IO;
}

static nx_result_t async_submit(void* context, nx_block_request_t* request) {
    nx_stm32_sdio_state_t* state = context;
    nx_result_t result = operation_valid(state, request->base.deadline);
    if (result != NX_SUCCESS)
        return result;
    const nx_stm32_sdio_config_t* config = state->config;
    if (config->engine == NX_STM32_SDIO_POLL || config->async == NULL)
        return NX_ERROR_UNSUPPORTED;
    if (state->state != NX_BLOCK_READY || !state->geometry_valid ||
        state->removed)
        return NX_ERROR_STATE;
    size_t length = (size_t)request->blocks * 512U;
    if (!cpu_storage(request, sizeof(*request), _Alignof(nx_block_request_t)) ||
        !valid_buffer(state, request, sizeof(*request)) ||
        !cpu_write_span(request, sizeof(*request)) || request->blocks == 0U ||
        request->blocks > config->max_blocks || request->data_bytes < length ||
        !valid_buffer(state, request->data, length) ||
        (!request->write && !cpu_write_span(request->data, length)) ||
        overlaps((uintptr_t)request->data, length, request, sizeof(*request)) ||
        overlaps((uintptr_t)request->data, length, config->async,
                 sizeof(*config->async)) ||
        overlaps((uintptr_t)request, sizeof(*request), config->async,
                 sizeof(*config->async)) ||
        request->first_block >= state->geometry.block_count ||
        request->blocks > state->geometry.block_count - request->first_block ||
        (state->high_capacity &&
         request->first_block + request->blocks - 1U > UINT32_MAX) ||
        (!state->high_capacity &&
         request->first_block + request->blocks - 1U > UINT32_MAX / 512U))
        return NX_ERROR_INVALID;
    if (request->write && !state->geometry.writable)
        return NX_ERROR_PERMISSION;
    if (nx_request_state(&request->base) != NX_REQUEST_READY)
        return NX_ERROR_STATE;
    if (config->engine == NX_STM32_SDIO_DMA) {
        if ((NX_SDIO_READ(RCC->AHB1ENR) & RCC_AHB1ENR_DMA2EN) == 0U ||
            LL_DMA_IsEnabledStream(DMA2, LL_DMA_STREAM_3) != 0U)
            return NX_ERROR_BUSY;
        result = nx_dma_buffer_validate(
            config->dma_regions, config->dma_region_count, request->data,
            length, 16U,
            request->write ? NX_DMA_TO_DEVICE : NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS)
            return result;
    }
    if (nx_time_now_us() >= request->base.deadline)
        return NX_ERROR_TIMEOUT;
    result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS)
        return result;
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    NX_SDIO_WRITE(SDIO->MASK, 0U);
    *config->async =
        (nx_stm32_sdio_async_state_t){.active = request, .unit_bytes = 512U};
    state->state = NX_BLOCK_ACTIVE;
    nx_arch_irq_restore(previous);
    result = async_start_block(state);
    if (result != NX_SUCCESS)
        async_drain(state, result);
    return NX_SUCCESS;
}

static nx_result_t async_cancel(void* context, nx_block_request_t* request) {
    nx_stm32_sdio_state_t* state = context;
    if (!allowed_context())
        return NX_ERROR_CONTEXT;
    if (state->config == NULL || state->config->async == NULL ||
        state->config->async->active != request)
        return NX_ERROR_STATE;
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    async_drain(state, NX_ERROR_CANCELLED);
    nx_arch_irq_restore(previous);
    return NX_SUCCESS;
}

static void async_service(void* context) {
    nx_stm32_sdio_state_t* state = context;
    if (!allowed_context() || state->config == NULL ||
        state->config->async == NULL || state->config->async->active == NULL)
        return;
    nx_stm32_sdio_async_state_t* execution = state->config->async;
    nx_block_request_t* request = execution->active;
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    uint32_t flags = execution->flags;
    uint32_t words = execution->words;
    bool memory_done = state->config->engine == NX_STM32_SDIO_DMA
                           ? execution->dma_complete
                           : words == (execution->unit_bytes + 3U) / 4U;
    if ((flags & DATA_ERRORS) != 0U)
        async_drain(state, (flags & SDIO_STA_DTIMEOUT) != 0U ? NX_ERROR_TIMEOUT
                                                             : NX_ERROR_IO);
    nx_arch_irq_restore(previous);
    if (!execution->draining && nx_time_now_us() >= request->base.deadline)
        async_drain(state, NX_ERROR_TIMEOUT);
    if (!execution->draining && memory_done &&
        (flags & (SDIO_STA_DATAEND | SDIO_STA_DBCKEND)) ==
            (SDIO_STA_DATAEND | SDIO_STA_DBCKEND) &&
        host_idle() && NX_SDIO_READ(SDIO->DCOUNT) == 0U) {
        NX_SDIO_WRITE(SDIO->MASK, 0U);
        sdio_poll_t poll = {request->base.deadline, state->config->poll_budget,
                            0U};
        nx_result_t result = host_withdraw(&poll, &execution->flags);
        bool memory_detached = dma_detach(state);
        if (!memory_detached || (execution->flags & DATA_ERRORS) != 0U)
            result = NX_ERROR_IO;
        if (result == NX_SUCCESS && request->write && execution->function == 0U)
            result = card_ready(state, &poll, false);
        if (result != NX_SUCCESS) {
            async_drain(state, result);
        } else {
            if (execution->function != 0U)
                state->config->io->inflight = 0U;
            ++execution->completed_blocks;
            if (execution->completed_blocks == request->blocks)
                async_drain(state, NX_SUCCESS);
            else {
                result = async_start_block(state);
                if (result != NX_SUCCESS)
                    async_drain(state, result);
                return;
            }
        }
    }
    if (!execution->draining)
        return;
    NX_SDIO_WRITE(SDIO->MASK, 0U);
    nx_time_us_t deadline = 0U;
    nx_result_t result = nx_deadline_finite_after(
        nx_time_now_us(), state->config->stop_timeout_us, &deadline);
    uint32_t restore_mask = 0U;
    sdio_poll_t poll = {deadline, state->config->poll_budget, 0U};
    if (result == NX_SUCCESS) {
        result = host_withdraw(&poll, &execution->flags);
    }
    /* Withdraw the peripheral request producer before disabling its memory
     * consumer. Even a failed host drain still attempts DMA isolation. */
    bool detached = dma_detach(state);
    if (result == NX_SUCCESS && detached) {
        if (execution->terminal == NX_SUCCESS &&
            (execution->flags & DATA_ERRORS) == 0U &&
            state->config->io != NULL && state->config->io->interrupt_armed &&
            state->config->engine != NX_STM32_SDIO_POLL) {
            result = control_settled(&poll);
            if (result == NX_SUCCESS) {
                NX_SDIO_WRITE(SDIO->DCTRL, SDIO_DCTRL_SDIOEN);
                nx_arch_dsb();
                if (NX_SDIO_READ(SDIO->DCTRL) != SDIO_DCTRL_SDIOEN)
                    result = NX_ERROR_IO;
                else
                    restore_mask = SDIO_STA_SDIOIT;
            }
        }
    }
    previous = nx_arch_irq_save();
    if (state->config->engine != NX_STM32_SDIO_POLL)
        NVIC_ClearPendingIRQ(SDIO_IRQn);
    nx_arch_dsb();
    bool irq_detached = state->config->engine == NX_STM32_SDIO_POLL ||
                        (NVIC_GetPendingIRQ(SDIO_IRQn) == 0U &&
                         NVIC_GetActive(SDIO_IRQn) == 0U);
    detached = detached && result == NX_SUCCESS &&
               NX_SDIO_READ(SDIO->MASK) == 0U && irq_detached;
    if (detached && restore_mask != 0U) {
        /* Join the old data-vector epoch before arming an independent
         * function hint. Global masking keeps that new hint from observing
         * the request until its active reference is cleared below. */
        NX_SDIO_WRITE(SDIO->MASK, restore_mask);
        nx_arch_dsb();
        detached = NX_SDIO_READ(SDIO->MASK) == restore_mask;
    }
    if (!detached) {
        (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
        nx_arch_irq_restore(previous);
        return;
    }
    nx_result_t terminal = execution->terminal;
    if (terminal == NX_SUCCESS && (execution->flags & DATA_ERRORS) != 0U) {
        terminal = NX_ERROR_IO;
        if (execution->completed_blocks != 0U)
            --execution->completed_blocks;
    }
    if (terminal != NX_SUCCESS && state->config->io != NULL)
        state->config->io->interrupt_armed = false;
    size_t transferred =
        (size_t)execution->completed_blocks * execution->unit_bytes;
    execution->active = NULL;
    execution->draining = false;
    state->state = terminal == NX_SUCCESS ? NX_BLOCK_READY : NX_BLOCK_UNCERTAIN;
    /* Every IRQ and hardware buffer reference is gone before this final
     * release publication. A late vector sees only the stable empty owner. */
    nx_request_settle(&request->base, terminal, transferred);
    nx_arch_irq_restore(previous);
}

static nx_result_t media_removed(void* context) {
    nx_stm32_sdio_state_t* state = context;
    if (!allowed_context())
        return NX_ERROR_CONTEXT;
    if (state->config == NULL || !state->infrastructure)
        return NX_ERROR_STATE;
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    state->removed = true;
    state->geometry_valid = false;
    if (state->config->async != NULL && state->config->async->active != NULL)
        async_drain(state, NX_ERROR_IO);
    else
        state->state = NX_BLOCK_UNCERTAIN;
    nx_arch_irq_restore(previous);
    return NX_SUCCESS;
}

const nx_block_async_ops_t nx_stm32_sdio_async_ops = {
    .submit = async_submit,
    .cancel = async_cancel,
    .service = async_service,
    .media_removed = media_removed,
};

static void data_step(nx_stm32_sdio_state_t* state) {
    if (state == NULL || state->config == NULL ||
        state->config->async == NULL ||
        (state->config->engine != NX_STM32_SDIO_POLL &&
         NX_SDIO_READ(SDIO->MASK) == 0U))
        return;
    nx_stm32_sdio_async_state_t* execution = state->config->async;
    uint32_t status = NX_SDIO_READ(SDIO->STA);
    if (state->config->io != NULL &&
        (status & NX_SDIO_READ(SDIO->MASK) & SDIO_STA_SDIOIT) != 0U) {
        state->config->io->interrupt_latched = true;
        state->config->io->interrupt_armed = false;
        NX_SDIO_WRITE(SDIO->MASK,
                      NX_SDIO_READ(SDIO->MASK) & ~(uint32_t)SDIO_STA_SDIOIT);
        NX_SDIO_WRITE(SDIO->ICR, SDIO_STA_SDIOIT);
    }
    if (execution->active == NULL || execution->draining)
        return;
    execution->flags |= status & DATA_FLAGS;
    if ((status & DATA_ERRORS) != 0U) {
        NX_SDIO_WRITE(SDIO->MASK, 0U);
        return;
    }
    if (state->config->engine != NX_STM32_SDIO_DMA) {
        nx_block_request_t* request = execution->active;
        uint8_t* data =
            (uint8_t*)request->data +
            (size_t)execution->completed_blocks * execution->unit_bytes;
        uint32_t words = (execution->unit_bytes + 3U) / 4U;
        for (unsigned count = 0U; count < 8U && execution->words < words;
             ++count) {
            status = NX_SDIO_READ(SDIO->STA);
            execution->flags |= status & DATA_FLAGS;
            if ((status & DATA_ERRORS) != 0U)
                break;
            uint32_t word = 0U;
            size_t bytes = execution->unit_bytes - execution->words * 4U;
            if (bytes > 4U)
                bytes = 4U;
            if (!request->write && (status & SDIO_STA_RXDAVL) != 0U) {
                word = NX_SDIO_FIFO_READ(SDIO);
                memcpy(data + (size_t)execution->words * 4U, &word, bytes);
            } else if (request->write && (status & SDIO_STA_TXFIFOHE) != 0U) {
                memcpy(&word, data + (size_t)execution->words * 4U, bytes);
                if (NX_SDIO_FIFO_WRITE(SDIO, &word) != HAL_OK) {
                    execution->flags |= SDIO_STA_TXUNDERR;
                    break;
                }
            } else {
                break;
            }
            ++execution->words;
        }
        status = NX_SDIO_READ(SDIO->STA);
        execution->flags |= status & DATA_FLAGS;
        if (execution->words == words) {
            NX_SDIO_WRITE(SDIO->MASK,
                          NX_SDIO_READ(SDIO->MASK) &
                              ~(uint32_t)(SDIO_STA_RXDAVL | SDIO_STA_TXFIFOHE));
            if (!request->write && (status & SDIO_STA_RXDAVL) != 0U)
                execution->flags |= SDIO_STA_RXOVERR;
        }
    }
    if ((execution->flags & DATA_ERRORS) != 0U ||
        ((execution->flags & (SDIO_STA_DATAEND | SDIO_STA_DBCKEND)) ==
             (SDIO_STA_DATAEND | SDIO_STA_DBCKEND) &&
         (state->config->engine == NX_STM32_SDIO_DMA ||
          execution->words == (execution->unit_bytes + 3U) / 4U)))
        NX_SDIO_WRITE(SDIO->MASK, 0U);
}

void nx_stm32_sdio_irq(nx_stm32_sdio_state_t* state) {
    if (state == NULL || state->config == NULL ||
        state->config->engine == NX_STM32_SDIO_POLL)
        return;
    data_step(state);
}

void nx_stm32_sdio_dma_irq(nx_stm32_sdio_state_t* state) {
    if (state == NULL || state->config == NULL ||
        state->config->engine != NX_STM32_SDIO_DMA ||
        state->config->async == NULL || state->config->async->active == NULL ||
        state->config->async->draining)
        return;
    if ((NX_SDIO_READ(DMA2_Stream3->CR) &
         (DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE)) == 0U &&
        (NX_SDIO_READ(DMA2_Stream3->FCR) & DMA_SxFCR_FEIE) == 0U)
        return;
    nx_stm32_sdio_async_state_t* execution = state->config->async;
    uint32_t flags = NX_SDIO_READ(DMA2->LISR) & NX_SDIO_DMA_FLAGS;
    if ((flags & NX_SDIO_DMA_ERRORS) != 0U)
        execution->flags |= SDIO_STA_RXOVERR;
    if ((flags & (UINT32_C(1) << 27U)) != 0U &&
        NX_SDIO_READ(DMA2_Stream3->NDTR) == 0U)
        execution->dma_complete = true;
    NX_SDIO_WRITE(DMA2->LIFCR, flags);
}

static nx_result_t io_check(nx_stm32_sdio_state_t* state, nx_time_us_t deadline,
                            bool ready) {
    nx_result_t result = operation_valid(state, deadline);
    if (result != NX_SUCCESS)
        return result;
    if (state->config->protocol != NX_STM32_SDIO_IO ||
        state->config->io == NULL)
        return NX_ERROR_UNSUPPORTED;
    if (!state->selected || state->removed ||
        state->config->io->function_count == 0U ||
        (ready && state->state != NX_BLOCK_READY))
        return NX_ERROR_STATE;
    if (ready &&
        (!host_idle() || (NX_SDIO_READ(SDIO->DCTRL) & SDIO_DCTRL_DTEN) != 0U))
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}

static bool io_output(const nx_stm32_sdio_state_t* state, const void* pointer,
                      size_t size) {
    return buffer_at(state, pointer, size, 1U) && cpu_write_span(pointer, size);
}

static nx_result_t io_begin(nx_stm32_sdio_state_t* state,
                            nx_time_us_t deadline) {
    if (nx_time_now_us() >= deadline)
        return NX_ERROR_TIMEOUT;
    state->state = NX_BLOCK_ACTIVE;
    return NX_SUCCESS;
}

static nx_result_t io_end(nx_stm32_sdio_state_t* state, nx_result_t result,
                          nx_time_us_t deadline) {
    if (result == NX_SUCCESS && nx_time_now_us() >= deadline)
        result = NX_ERROR_TIMEOUT;
    if (result != NX_SUCCESS) {
        nx_arch_irq_state_t previous = nx_arch_irq_save();
        state->config->io->interrupt_armed = false;
        NX_SDIO_WRITE(SDIO->MASK, 0U);
        nx_arch_irq_restore(previous);
        nx_time_us_t drain;
        if (nx_deadline_finite_after(nx_time_now_us(),
                                     state->config->stop_timeout_us,
                                     &drain) == NX_SUCCESS) {
            sdio_poll_t poll = {drain, state->config->poll_budget, 0U};
            (void)host_withdraw(&poll, NULL);
        }
    }
    state->state = result == NX_SUCCESS ? NX_BLOCK_READY : NX_BLOCK_UNCERTAIN;
    return result;
}

static bool io_function(const nx_stm32_sdio_state_t* state, uint8_t function,
                        bool zero) {
    return (zero || function != 0U) &&
           function <= state->config->io->function_count;
}

static nx_result_t io_pointer(uint32_t address, uint32_t* pointer,
                              sdio_poll_t* poll) {
    uint32_t value = 0U;
    for (unsigned byte = 0U; byte < 3U; ++byte) {
        uint8_t part = 0U;
        nx_result_t result =
            io_register(0U, address + byte, &part, false, false, poll);
        if (result != NX_SUCCESS)
            return result;
        value |= (uint32_t)part << (8U * byte);
    }
    if (value > UINT32_C(0x1ffff))
        return NX_ERROR_UNSUPPORTED;
    *pointer = value;
    return NX_SUCCESS;
}

static nx_result_t io_fbr_size(uint8_t function, uint16_t* size,
                               sdio_poll_t* poll) {
    uint8_t low = 0U, high = 0U;
    uint32_t address = (uint32_t)function * 256U + 16U;
    nx_result_t result = io_register(0U, address, &low, false, false, poll);
    if (result == NX_SUCCESS)
        result = io_register(0U, address + 1U, &high, false, false, poll);
    if (result == NX_SUCCESS)
        *size = (uint16_t)((uint16_t)low | ((uint16_t)high << 8U));
    return result;
}

static bool io_block_bytes(uint16_t bytes) {
    return bytes != 0U && bytes <= 2048U && (bytes & (bytes - 1U)) == 0U;
}

static nx_result_t io_inspect(void* context, nx_sdio_io_info_t* output,
                              nx_time_us_t deadline) {
    nx_stm32_sdio_state_t* state = context;
    nx_result_t result = io_check(state, deadline, true);
    if (result != NX_SUCCESS)
        return result;
    if (!io_output(state, output, sizeof(*output)))
        return NX_ERROR_INVALID;
    result = io_begin(state, deadline);
    if (result != NX_SUCCESS)
        return result;
    nx_sdio_io_info_t info = {0};
    info.function_count = state->config->io->function_count;
    info.memory_present = state->config->io->memory_present;
    sdio_poll_t poll = {deadline, state->config->poll_budget, 0U};
    result = io_pointer(9U, &info.common_cis_address, &poll);
    if (result == NX_SUCCESS)
        result =
            io_register(0U, 2U, &info.enabled_functions, false, false, &poll);
    if (result == NX_SUCCESS)
        result =
            io_register(0U, 4U, &info.interrupt_functions, false, false, &poll);
    info.interrupt_functions &= io_function_mask(state);
    for (uint8_t function = 1U;
         function <= info.function_count && result == NX_SUCCESS; ++function) {
        nx_sdio_function_info_t* entry = &info.functions[function - 1U];
        uint32_t fbr = (uint32_t)function * 256U;
        result = io_pointer(fbr + 9U, &entry->cis_address, &poll);
        if (result == NX_SUCCESS)
            result = io_fbr_size(function, &entry->block_bytes, &poll);
        if (result == NX_SUCCESS)
            result = io_register(0U, fbr, &entry->interface_code, false, false,
                                 &poll);
        entry->interface_code &= 15U;
        if (result == NX_SUCCESS && entry->interface_code == 15U)
            result = io_register(0U, fbr + 1U, &entry->interface_code, false,
                                 false, &poll);
    }
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    info.interrupt_latched = state->config->io->interrupt_latched;
    info.interrupt_armed = state->config->io->interrupt_armed &&
                           (NX_SDIO_READ(SDIO->MASK) & SDIO_STA_SDIOIT) != 0U;
    nx_arch_irq_restore(previous);
    result = io_end(state, result, deadline);
    if (result == NX_SUCCESS)
        *output = info;
    return result;
}

static nx_result_t io_enable(void* context, uint8_t function, bool enabled,
                             nx_time_us_t deadline) {
    nx_stm32_sdio_state_t* state = context;
    nx_result_t result = io_check(state, deadline, true);
    if (result != NX_SUCCESS)
        return result;
    if (!io_function(state, function, false))
        return NX_ERROR_INVALID;
    result = io_begin(state, deadline);
    if (result != NX_SUCCESS)
        return result;
    uint8_t mask = (uint8_t)(1U << function);
    uint8_t desired = enabled ? (uint8_t)(state->config->io->enabled | mask)
                              : (uint8_t)(state->config->io->enabled & ~mask);
    sdio_poll_t poll = {deadline, state->config->poll_budget, 0U};
    result = io_program(2U, desired, &poll);
    if (result == NX_SUCCESS) {
        state->config->io->enabled = desired;
        result = io_wait_ready(state, desired, &poll);
    }
    return io_end(state, result, deadline);
}

static nx_result_t io_set_size(void* context, uint8_t function, uint16_t bytes,
                               nx_time_us_t deadline) {
    nx_stm32_sdio_state_t* state = context;
    nx_result_t result = io_check(state, deadline, true);
    if (result != NX_SUCCESS)
        return result;
    if (!io_function(state, function, false) || !io_block_bytes(bytes))
        return NX_ERROR_INVALID;
    result = io_begin(state, deadline);
    if (result != NX_SUCCESS)
        return result;
    state->config->io->block_bytes[function - 1U] = 0U;
    sdio_poll_t poll = {deadline, state->config->poll_budget, 0U};
    uint32_t address = (uint32_t)function * 256U + 16U;
    result = io_program(address, (uint8_t)bytes, &poll);
    if (result == NX_SUCCESS)
        result = io_program(address + 1U, (uint8_t)(bytes >> 8U), &poll);
    result = io_end(state, result, deadline);
    if (result == NX_SUCCESS)
        state->config->io->block_bytes[function - 1U] = bytes;
    return result;
}

static nx_result_t io_read_byte(void* context, uint8_t function,
                                uint32_t address, uint8_t* output,
                                nx_time_us_t deadline) {
    nx_stm32_sdio_state_t* state = context;
    nx_result_t result = io_check(state, deadline, true);
    if (result != NX_SUCCESS)
        return result;
    if (!io_function(state, function, true) || address > UINT32_C(0x1ffff) ||
        !io_output(state, output, sizeof(*output)))
        return NX_ERROR_INVALID;
    if (function != 0U && (state->config->io->enabled & (1U << function)) == 0U)
        return NX_ERROR_STATE;
    result = io_begin(state, deadline);
    if (result != NX_SUCCESS)
        return result;
    sdio_poll_t poll = {deadline, state->config->poll_budget, 0U};
    uint8_t value = 0U;
    result = io_register(function, address, &value, false, false, &poll);
    result = io_end(state, result, deadline);
    if (result == NX_SUCCESS)
        *output = value;
    return result;
}

static nx_result_t io_write_byte(void* context, uint8_t function,
                                 uint32_t address, uint8_t value,
                                 nx_time_us_t deadline) {
    nx_stm32_sdio_state_t* state = context;
    nx_result_t result = io_check(state, deadline, true);
    if (result != NX_SUCCESS)
        return result;
    if (!io_function(state, function, false) || address > UINT32_C(0x1ffff))
        return NX_ERROR_INVALID;
    if ((state->config->io->enabled & (1U << function)) == 0U)
        return NX_ERROR_STATE;
    result = io_begin(state, deadline);
    if (result != NX_SUCCESS)
        return result;
    sdio_poll_t poll = {deadline, state->config->poll_budget, 0U};
    result = io_register(function, address, &value, true, false, &poll);
    return io_end(state, result, deadline);
}

static nx_result_t io_interrupts(void* context, uint8_t functions,
                                 nx_time_us_t deadline) {
    nx_stm32_sdio_state_t* state = context;
    nx_result_t result = io_check(state, deadline, true);
    if (result != NX_SUCCESS)
        return result;
    if ((functions & (uint8_t)~io_function_mask(state)) != 0U)
        return NX_ERROR_INVALID;
    result = io_begin(state, deadline);
    if (result != NX_SUCCESS)
        return result;
    sdio_poll_t poll = {deadline, state->config->poll_budget, 0U};
    result = io_program(4U, (uint8_t)(functions | (functions != 0U ? 1U : 0U)),
                        &poll);
    if (result == NX_SUCCESS)
        result = control_settled(&poll);
    if (result == NX_SUCCESS) {
        uint32_t mask =
            functions != 0U && state->config->engine != NX_STM32_SDIO_POLL
                ? SDIO_STA_SDIOIT
                : 0U;
        nx_arch_irq_state_t previous = nx_arch_irq_save();
        state->config->io->interrupts = functions;
        state->config->io->interrupt_latched = false;
        state->config->io->interrupt_armed = mask != 0U;
        NX_SDIO_WRITE(SDIO->ICR, SDIO_STA_SDIOIT);
        NX_SDIO_WRITE(SDIO->DCTRL, SDIO_DCTRL_SDIOEN);
        NX_SDIO_WRITE(SDIO->MASK, mask);
        nx_arch_dsb();
        if (NX_SDIO_READ(SDIO->DCTRL) != SDIO_DCTRL_SDIOEN ||
            NX_SDIO_READ(SDIO->MASK) != mask)
            result = NX_ERROR_IO;
        nx_arch_irq_restore(previous);
    }
    return io_end(state, result, deadline);
}

static nx_result_t io_pending(void* context, uint8_t* output,
                              nx_time_us_t deadline) {
    nx_stm32_sdio_state_t* state = context;
    nx_result_t result = io_check(state, deadline, true);
    if (result != NX_SUCCESS)
        return result;
    if (!io_output(state, output, sizeof(*output)))
        return NX_ERROR_INVALID;
    result = io_begin(state, deadline);
    if (result != NX_SUCCESS)
        return result;
    sdio_poll_t poll = {deadline, state->config->poll_budget, 0U};
    uint8_t pending = 0U;
    result = io_register(0U, 5U, &pending, false, false, &poll);
    result = io_end(state, result, deadline);
    if (result == NX_SUCCESS)
        *output = pending & io_function_mask(state);
    return result;
}

static nx_result_t io_submit(void* context, nx_sdio_io_request_t* request) {
    nx_stm32_sdio_state_t* state = context;
    nx_block_request_t* transfer = &request->transfer;
    nx_result_t result = io_check(state, transfer->base.deadline, true);
    if (result != NX_SUCCESS)
        return result;
    const nx_stm32_sdio_config_t* config = state->config;
    if (!cpu_storage(request, sizeof(*request),
                     _Alignof(nx_sdio_io_request_t)) ||
        !valid_buffer(state, request, sizeof(*request)) ||
        !cpu_write_span(request, sizeof(*request)) ||
        !io_function(state, request->function, false) ||
        request->address > UINT32_C(0x1ffff) || request->unit_bytes == 0U ||
        transfer->first_block != 0U || transfer->blocks == 0U ||
        transfer->blocks > config->max_blocks ||
        (!request->block_mode &&
         (transfer->blocks != 1U || request->unit_bytes > 512U)) ||
        (request->block_mode && !io_block_bytes(request->unit_bytes)))
        return NX_ERROR_INVALID;
    if ((config->io->enabled & (1U << request->function)) == 0U ||
        (request->block_mode &&
         config->io->block_bytes[request->function - 1U] !=
             request->unit_bytes))
        return NX_ERROR_STATE;
    size_t length = (size_t)request->unit_bytes * transfer->blocks;
    if (transfer->data_bytes < length ||
        !valid_buffer(state, transfer->data, length) ||
        (!transfer->write && !cpu_write_span(transfer->data, length)) ||
        overlaps((uintptr_t)transfer->data, length, request,
                 sizeof(*request)) ||
        (request->increment && length > UINT32_C(0x20000) - request->address))
        return NX_ERROR_INVALID;
    if (nx_request_state(&transfer->base) != NX_REQUEST_READY)
        return NX_ERROR_STATE;
    if (config->engine == NX_STM32_SDIO_DMA) {
        if ((request->unit_bytes & 15U) != 0U)
            return NX_ERROR_UNSUPPORTED;
        if ((NX_SDIO_READ(RCC->AHB1ENR) & RCC_AHB1ENR_DMA2EN) == 0U ||
            LL_DMA_IsEnabledStream(DMA2, LL_DMA_STREAM_3) != 0U)
            return NX_ERROR_BUSY;
        result = nx_dma_buffer_validate(
            config->dma_regions, config->dma_region_count, transfer->data,
            length, 16U,
            transfer->write ? NX_DMA_TO_DEVICE : NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS)
            return result;
    }
    if (nx_time_now_us() >= transfer->base.deadline)
        return NX_ERROR_TIMEOUT;
    result = nx_request_admit(&transfer->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS)
        return result;
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    NX_SDIO_WRITE(SDIO->MASK, 0U);
    *config->async = (nx_stm32_sdio_async_state_t){
        .active = transfer,
        .function_address = request->address,
        .unit_bytes = request->unit_bytes,
        .function = request->function,
        .block_mode = request->block_mode,
        .increment = request->increment,
    };
    state->state = NX_BLOCK_ACTIVE;
    nx_arch_irq_restore(previous);
    result = async_start_block(state);
    if (result != NX_SUCCESS)
        async_drain(state, result);
    return NX_SUCCESS;
}

static nx_result_t io_cancel(void* context, nx_sdio_io_request_t* request) {
    return async_cancel(context, &request->transfer);
}

static void io_service(void* context) {
    nx_stm32_sdio_state_t* state = context;
    if (!allowed_context() || state->config == NULL ||
        state->config->protocol != NX_STM32_SDIO_IO)
        return;
    if (state->config->engine == NX_STM32_SDIO_POLL)
        data_step(state);
    async_service(state);
}

static nx_result_t io_sync(void* context, nx_time_us_t deadline) {
    nx_stm32_sdio_state_t* state = context;
    nx_result_t result = io_check(state, deadline, false);
    if (result != NX_SUCCESS)
        return result;
    result = io_begin(state, deadline);
    if (result != NX_SUCCESS)
        return result;
    state->config->io->interrupt_armed = false;
    NX_SDIO_WRITE(SDIO->MASK, 0U);
    sdio_poll_t poll = {deadline, state->config->poll_budget, 0U};
    result = host_withdraw(&poll, NULL);
    nx_stm32_sdio_io_state_t* io = state->config->io;
    if (result == NX_SUCCESS && io->inflight != 0U) {
        uint8_t abort = io->inflight;
        result = io_register(0U, 6U, &abort, true, false, &poll);
        if (result == NX_SUCCESS)
            io->inflight = 0U;
    }
    uint8_t enabled = 0U;
    if (result == NX_SUCCESS)
        result = io_register(0U, 2U, &enabled, false, false, &poll);
    enabled &= io_function_mask(state);
    if (result == NX_SUCCESS)
        result = io_wait_ready(state, enabled, &poll);
    for (uint8_t function = 1U;
         function <= io->function_count && result == NX_SUCCESS; ++function) {
        uint16_t bytes = 0U;
        result = io_fbr_size(function, &bytes, &poll);
        if (result == NX_SUCCESS)
            io->block_bytes[function - 1U] = io_block_bytes(bytes) ? bytes : 0U;
    }
    if (result == NX_SUCCESS)
        io->enabled = enabled;
    return io_end(state, result, deadline);
}

const nx_sdio_io_ops_t nx_stm32_sdio_io_ops = {
    .inspect = io_inspect,
    .enable = io_enable,
    .block_size = io_set_size,
    .read_byte = io_read_byte,
    .write_byte = io_write_byte,
    .interrupts = io_interrupts,
    .pending = io_pending,
    .submit = io_submit,
    .cancel = io_cancel,
    .service = io_service,
    .sync = io_sync,
};

static bool cold_disjoint(const nx_stm32_sdio_state_t* state,
                          const nx_stm32_sdio_config_t* config,
                          const nx_stm32_sdio_receipt_t* receipt) {
    uintptr_t start = (uintptr_t)state;
    uintptr_t acquired = (uintptr_t)receipt;
    if (config->io != NULL &&
        (overlaps((uintptr_t)config->io, sizeof(*config->io), state,
                  sizeof(*state)) ||
         overlaps((uintptr_t)config->io, sizeof(*config->io), receipt,
                  sizeof(*receipt)) ||
         overlaps((uintptr_t)config->io, sizeof(*config->io), config,
                  sizeof(*config)) ||
         overlaps((uintptr_t)config->io, sizeof(*config->io), config->pins,
                  pin_count(config) * sizeof(*config->pins)) ||
         (config->async != NULL &&
          overlaps((uintptr_t)config->io, sizeof(*config->io), config->async,
                   sizeof(*config->async))) ||
         (config->dma_region_count != 0U &&
          overlaps((uintptr_t)config->io, sizeof(*config->io),
                   config->dma_regions,
                   config->dma_region_count * sizeof(*config->dma_regions)))))
        return false;
    if (config->async != NULL &&
        (overlaps((uintptr_t)config->async, sizeof(*config->async), state,
                  sizeof(*state)) ||
         overlaps((uintptr_t)config->async, sizeof(*config->async), receipt,
                  sizeof(*receipt)) ||
         overlaps((uintptr_t)config->async, sizeof(*config->async), config,
                  sizeof(*config)) ||
         overlaps((uintptr_t)config->async, sizeof(*config->async),
                  config->pins, pin_count(config) * sizeof(*config->pins))))
        return false;
    if (overlaps(start, sizeof(*state), receipt, sizeof(*receipt)) ||
        overlaps(start, sizeof(*state), config, sizeof(*config)) ||
        overlaps(acquired, sizeof(*receipt), config, sizeof(*config)) ||
        overlaps(start, sizeof(*state), config->pins,
                 pin_count(config) * sizeof(*config->pins)) ||
        overlaps(acquired, sizeof(*receipt), config->pins,
                 pin_count(config) * sizeof(*config->pins)))
        return false;
    for (size_t i = 0U; i < pin_count(config); ++i) {
        uintptr_t snapshot = (uintptr_t)config->pins[i].snapshot;
        if (overlaps(start, sizeof(*state), (void*)snapshot,
                     sizeof(uint16_t)) ||
            overlaps(acquired, sizeof(*receipt), (void*)snapshot,
                     sizeof(uint16_t)) ||
            overlaps(snapshot, sizeof(uint16_t), config, sizeof(*config)) ||
            overlaps(snapshot, sizeof(uint16_t), config->pins,
                     pin_count(config) * sizeof(*config->pins)) ||
            (config->io != NULL && overlaps(snapshot, sizeof(uint16_t),
                                            config->io, sizeof(*config->io))) ||
            (config->async != NULL &&
             overlaps(snapshot, sizeof(uint16_t), config->async,
                      sizeof(*config->async))))
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
        receipt->pin_mask != 0U || receipt->clock_added != 0U ||
        receipt->vectors ||
        (config->async != NULL && config->async->active != NULL))
        return NX_ERROR_BUSY;
    const uint32_t gpio_clocks =
        RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN |
        (config->bus_width == 8U ? RCC_AHB1ENR_GPIOBEN : 0U);
    if (!live_clock() ||
        (NX_SDIO_READ(RCC->AHB1ENR) & gpio_clocks) != gpio_clocks)
        return NX_ERROR_STATE;
    uint32_t bank = (uint32_t)SDIO_IRQn / 32U;
    uint32_t irq_mask = UINT32_C(1) << ((uint32_t)SDIO_IRQn % 32U);
    if (((NVIC->ISER[bank] | NVIC->ISPR[bank] | NVIC->IABR[bank]) & irq_mask) !=
        0U)
        return NX_ERROR_BUSY;
    if (config->engine == NX_STM32_SDIO_DMA) {
        if ((NX_SDIO_READ(RCC->AHB1ENR) & RCC_AHB1ENR_DMA2EN) == 0U)
            return NX_ERROR_STATE;
        if (NVIC_GetEnableIRQ(DMA2_Stream3_IRQn) != 0U ||
            NVIC_GetPendingIRQ(DMA2_Stream3_IRQn) != 0U ||
            NVIC_GetActive(DMA2_Stream3_IRQn) != 0U ||
            LL_DMA_IsEnabledStream(DMA2, LL_DMA_STREAM_3) != 0U)
            return NX_ERROR_BUSY;
    }
    if ((NX_SDIO_READ(RCC->APB2ENR) & RCC_APB2ENR_SDIOEN) != 0U &&
        (NX_SDIO_READ(SDIO->POWER) != 0U ||
         (NX_SDIO_READ(SDIO->CLKCR) & SDIO_CLKCR_CLKEN) != 0U ||
         NX_SDIO_READ(SDIO->MASK) != 0U || NX_SDIO_READ(SDIO->DCTRL) != 0U ||
         !host_idle()))
        return NX_ERROR_BUSY;
    const nx_stm32_platform_config_t pads = {.pins = config->pins,
                                             .pin_count = pin_count(config)};
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
    for (size_t i = 0U; i < pin_count(config); ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        *pin->snapshot = nx_stm32_pin_capture(pin->registers, pin->pin);
        receipt->pin_mask |= (uint16_t)(1U << i);
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
    if (config->engine != NX_STM32_SDIO_POLL) {
        receipt->sdio_priority = (uint8_t)NVIC_GetPriority(SDIO_IRQn);
        receipt->dma_priority = (uint8_t)NVIC_GetPriority(DMA2_Stream3_IRQn);
        receipt->vectors = true;
        NVIC_SetPriority(SDIO_IRQn, config->irq_priority);
        NVIC_EnableIRQ(SDIO_IRQn);
        if (config->engine == NX_STM32_SDIO_DMA) {
            NX_SDIO_WRITE(DMA2->LIFCR, NX_SDIO_DMA_FLAGS);
            NVIC_SetPriority(DMA2_Stream3_IRQn, config->irq_priority);
            NVIC_EnableIRQ(DMA2_Stream3_IRQn);
        }
        nx_arch_dsb();
        if (NVIC_GetEnableIRQ(SDIO_IRQn) == 0U ||
            NVIC_GetPriority(SDIO_IRQn) != config->irq_priority ||
            (config->engine == NX_STM32_SDIO_DMA &&
             (NVIC_GetEnableIRQ(DMA2_Stream3_IRQn) == 0U ||
              NVIC_GetPriority(DMA2_Stream3_IRQn) != config->irq_priority)))
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
    const uint16_t acquired_pins =
        (uint16_t)((UINT32_C(1) << pin_count(config)) - 1U);
    if (state->config != config ||
        (receipt->pin_mask & (uint16_t)~acquired_pins) != 0U ||
        (receipt->clock_added & ~RCC_APB2ENR_SDIOEN) != 0U)
        return NX_ERROR_INVALID;
    if (state->state == NX_BLOCK_ACTIVE ||
        (config->async != NULL && config->async->active != NULL))
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
    if (receipt->vectors) {
        NX_SDIO_WRITE(SDIO->MASK, 0U);
        NVIC_DisableIRQ(SDIO_IRQn);
        NVIC_ClearPendingIRQ(SDIO_IRQn);
        if (config->engine == NX_STM32_SDIO_DMA) {
            if (!dma_detach(state))
                return NX_ERROR_IO;
            NVIC_DisableIRQ(DMA2_Stream3_IRQn);
            NVIC_ClearPendingIRQ(DMA2_Stream3_IRQn);
        }
        nx_arch_dsb();
        if (NX_SDIO_READ(SDIO->MASK) != 0U ||
            NVIC_GetEnableIRQ(SDIO_IRQn) != 0U ||
            NVIC_GetPendingIRQ(SDIO_IRQn) != 0U ||
            NVIC_GetActive(SDIO_IRQn) != 0U ||
            (config->engine == NX_STM32_SDIO_DMA &&
             (NVIC_GetEnableIRQ(DMA2_Stream3_IRQn) != 0U ||
              NVIC_GetPendingIRQ(DMA2_Stream3_IRQn) != 0U ||
              NVIC_GetActive(DMA2_Stream3_IRQn) != 0U)))
            return NX_ERROR_IO;
        NVIC_SetPriority(SDIO_IRQn, receipt->sdio_priority);
        if (config->engine == NX_STM32_SDIO_DMA)
            NVIC_SetPriority(DMA2_Stream3_IRQn, receipt->dma_priority);
        receipt->vectors = false;
    }
    for (size_t i = pin_count(config); i != 0U; --i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i - 1U];
        uint16_t mask = (uint16_t)(1U << (i - 1U));
        if ((receipt->pin_mask & mask) == 0U)
            continue;
        nx_stm32_pin_restore(pin->registers, pin->pin, *pin->snapshot);
        nx_arch_dsb();
        if (nx_stm32_pin_capture(pin->registers, pin->pin) != *pin->snapshot)
            return NX_ERROR_IO;
        receipt->pin_mask &= (uint16_t)~mask;
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
