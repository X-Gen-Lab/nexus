/**
 * \file            transport.c
 * \brief           Bounded F103ZE SDIO command and single-sector data paths
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "transport.h"

#include "nexus_config.h"
#include "private/hardware.h"
#include "system/platform.h"
#include <string.h>

#if !defined(STM32F103xE) ||                                                   \
    (!defined(NEXUS_CMSIS_HOST_MODEL) &&                                       \
     (NEXUS_CORE_HZ != 8000000U || NEXUS_CLOCK_APB2_HZ != 8000000U))
#error "This SDIO transport requires the reviewed F103xE HSI8 clock plan"
#endif

#ifndef NX_STM32F1_SDIO_FIFO_READ
#define NX_STM32F1_SDIO_FIFO_READ() (SDIO->FIFO)
#endif

enum {
    HOST_CLOSED,
    HOST_GATE,
    HOST_LIVE,
    HOST_DRAIN,
    IDENT_DIV = 20U,
    SECTOR_BYTES = 512U,
    SECTOR_WORDS = SECTOR_BYTES / sizeof(uint32_t),
    CONTROL_US = 2U,
    POLL_LIMIT = 65535U,
    WRITE_TIMEOUT_CYCLES = 363636U,
};

#define HOST_ACTIVE (SDIO_STA_CMDACT | SDIO_STA_TXACT | SDIO_STA_RXACT)
#define COMMAND_FLAGS                                                          \
    (SDIO_STA_CTIMEOUT | SDIO_STA_CCRCFAIL | SDIO_STA_CMDREND |                \
     SDIO_STA_CMDSENT)
#define DATA_ERRORS                                                            \
    (SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_TXUNDERR |               \
     SDIO_STA_RXOVERR | SDIO_STA_STBITERR)
#define DATA_FLAGS (DATA_ERRORS | SDIO_STA_DATAEND | SDIO_STA_DBCKEND)
#define RX_CONTROL                                                             \
    (SDIO_DCTRL_DTEN | SDIO_DCTRL_DTDIR | (9U << SDIO_DCTRL_DBLOCKSIZE_Pos))
#define TX_CONTROL     (SDIO_DCTRL_DTEN | (9U << SDIO_DCTRL_DBLOCKSIZE_Pos))
#define CARD_R1_ERRORS UINT32_C(0xffffe008)

/* ES0340: six parent cycles must be shorter than one 32-bit wire word. */
_Static_assert(6U < 32U * (IDENT_DIV + 2U), "SDIO one-bit TX underrun window");

typedef struct {
    nx_time_us_t deadline;
    uint32_t remaining;
} sdio_budget_t;

static bool budget_step(sdio_budget_t* budget) {
    if (budget->remaining == 0U ||
        nx_deadline_expired(budget->deadline, nx_time_now_us()))
        return false;
    --budget->remaining;
    return true;
}

/** \brief Space control writes across the HCLK and PCLK2 clock boundaries.
 * \note At HSI8 one full microsecond exceeds seven HCLK cycles. A further
 *       tick accounts for the floor-microsecond timestamp.
 */
static nx_result_t control_wait(sdio_budget_t* budget) {
    const nx_time_us_t start = nx_time_now_us();
    nx_time_us_t settled;
    if (nx_deadline_finite_after(start, CONTROL_US, &settled) != NX_SUCCESS)
        return NX_ERROR_TIMEOUT;
    do {
        if (!budget_step(budget))
            return NX_ERROR_TIMEOUT;
    } while (nx_time_now_us() < settled);
    return NX_SUCCESS;
}

static nx_result_t valid_call(const nx_stm32f1_sdio_t* host,
                              nx_time_us_t deadline) {
    if (host == NULL || host->phase > HOST_DRAIN ||
        deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    return nx_deadline_expired(deadline, nx_time_now_us()) ? NX_ERROR_TIMEOUT
                                                           : NX_SUCCESS;
}

static bool clear_status(uint32_t flags) {
    NX_STM32F1_WRITE(&SDIO->ICR, flags);
    nx_arch_dsb();
    return (SDIO->STA & flags) == 0U;
}

static bool host_idle(void) {
    return (SDIO->STA & HOST_ACTIVE) == 0U;
}

static nx_result_t release_gate(nx_stm32f1_sdio_t* host) {
    if (!nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_SDIOEN, 0U))
        return NX_ERROR_IO;
    host->phase = HOST_CLOSED;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_sdio_open(nx_stm32f1_sdio_t* host,
                                 nx_time_us_t deadline) {
    nx_result_t result = valid_call(host, deadline);
    if (result != NX_SUCCESS)
        return result;
    if (host->phase != HOST_CLOSED || !nx_stm32f1_system_live())
        return NX_ERROR_STATE;
    if ((RCC->AHBENR & RCC_AHBENR_SDIOEN) != 0U)
        return NX_ERROR_BUSY;

    host->phase = HOST_GATE;
    if (!nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_SDIOEN,
                                 RCC_AHBENR_SDIOEN))
        return NX_ERROR_IO;
    if (!host_idle() || SDIO->POWER != 0U || SDIO->CLKCR != 0U ||
        SDIO->CMD != 0U || SDIO->DCTRL != 0U || SDIO->MASK != 0U ||
        (SDIO->STA & SDIO_STA_RXDAVL) != 0U) {
        result = release_gate(host);
        return result == NX_SUCCESS ? NX_ERROR_BUSY : result;
    }

    sdio_budget_t budget = {deadline, POLL_LIMIT};
    host->phase = HOST_DRAIN;
    if (!nx_stm32f1_write_masked(&SDIO->POWER, SDIO_POWER_PWRCTRL,
                                 SDIO_POWER_PWRCTRL))
        return NX_ERROR_IO;
    result = control_wait(&budget);
    if (result != NX_SUCCESS)
        return result;
    NX_STM32F1_WRITE(&SDIO->CLKCR, SDIO_CLKCR_CLKEN | IDENT_DIV);
    nx_arch_dsb();
    if (SDIO->CLKCR != (SDIO_CLKCR_CLKEN | IDENT_DIV))
        return NX_ERROR_IO;
    result = control_wait(&budget);
    if (result == NX_SUCCESS)
        host->phase = HOST_LIVE;
    return result;
}

static nx_result_t command(uint8_t index, uint32_t argument,
                           nx_stm32f1_sdio_response_t response,
                           nx_stm32f1_sdio_reply_t* reply,
                           sdio_budget_t* budget) {
    nx_result_t result = control_wait(budget);
    if (result != NX_SUCCESS)
        return result;
    if (!clear_status(COMMAND_FLAGS))
        return NX_ERROR_IO;
    NX_STM32F1_WRITE(&SDIO->ARG, argument);
    const uint32_t wait_response = response == NX_STM32F1_SDIO_NONE ? 0U
                                   : response == NX_STM32F1_SDIO_LONG
                                       ? SDIO_CMD_WAITRESP
                                       : SDIO_CMD_WAITRESP_0;
    NX_STM32F1_WRITE(&SDIO->CMD,
                     (uint32_t)index | wait_response | SDIO_CMD_CPSMEN);

    uint32_t status;
    do {
        if (!budget_step(budget))
            return NX_ERROR_TIMEOUT;
        status = SDIO->STA;
    } while ((status & COMMAND_FLAGS) == 0U ||
             (status & SDIO_STA_CMDACT) != 0U);
    if (!clear_status(status & COMMAND_FLAGS))
        return NX_ERROR_IO;
    if ((status & SDIO_STA_CTIMEOUT) != 0U)
        return NX_ERROR_TIMEOUT;

    if (response == NX_STM32F1_SDIO_NONE)
        return (status & SDIO_STA_CMDSENT) != 0U ? NX_SUCCESS : NX_ERROR_IO;

    const uint32_t expected = response == NX_STM32F1_SDIO_OCR
                                  ? SDIO_STA_CMDREND | SDIO_STA_CCRCFAIL
                                  : SDIO_STA_CMDREND;
    if ((status & expected) == 0U ||
        ((status & SDIO_STA_CCRCFAIL) != 0U &&
         response != NX_STM32F1_SDIO_OCR) ||
        (response == NX_STM32F1_SDIO_SHORT &&
         (SDIO->RESPCMD & SDIO_RESPCMD_RESPCMD) != index))
        return NX_ERROR_IO;
    const bool long_reply = response == NX_STM32F1_SDIO_LONG;
    *reply = (nx_stm32f1_sdio_reply_t){
        {SDIO->RESP1, long_reply ? SDIO->RESP2 : 0U,
         long_reply ? SDIO->RESP3 : 0U, long_reply ? SDIO->RESP4 : 0U}};
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_sdio_command(nx_stm32f1_sdio_t* host, uint8_t index,
                                    uint32_t argument,
                                    nx_stm32f1_sdio_response_t response,
                                    nx_stm32f1_sdio_reply_t* reply,
                                    nx_time_us_t deadline) {
    nx_result_t result = valid_call(host, deadline);
    if (result != NX_SUCCESS)
        return result;
    if (index > 63U || (unsigned)response > NX_STM32F1_SDIO_LONG ||
        (response != NX_STM32F1_SDIO_NONE && reply == NULL))
        return NX_ERROR_INVALID;
    if (host->phase != HOST_LIVE || !nx_stm32f1_system_live())
        return NX_ERROR_STATE;
    if ((RCC->AHBENR & RCC_AHBENR_SDIOEN) == 0U)
        return NX_ERROR_IO;
    if (!host_idle() || SDIO->DCTRL != 0U)
        return NX_ERROR_BUSY;
    sdio_budget_t budget = {deadline, POLL_LIMIT};
    result = command(index, argument, response, reply, &budget);
    if (!host_idle())
        host->phase = HOST_DRAIN;
    return result;
}

static nx_result_t receive_words(uint8_t* data, sdio_budget_t* budget) {
    size_t count = 0U;
    do {
        if (!budget_step(budget))
            return NX_ERROR_TIMEOUT;
        const uint32_t status = SDIO->STA;
        if ((status & DATA_ERRORS) != 0U)
            return (status & SDIO_STA_DTIMEOUT) != 0U ? NX_ERROR_TIMEOUT
                                                      : NX_ERROR_IO;
        if ((status & SDIO_STA_RXDAVL) != 0U) {
            if (count == SECTOR_WORDS)
                return NX_ERROR_IO;
            uint32_t word = NX_STM32F1_SDIO_FIFO_READ();
            memcpy(data + count * sizeof(word), &word, sizeof(word));
            ++count;
        } else if ((status & (SDIO_STA_DATAEND | SDIO_STA_RXACT)) ==
                   SDIO_STA_DATAEND) {
            if (count != SECTOR_WORDS || SDIO->DCOUNT != 0U)
                return NX_ERROR_IO;
            /* DATAEND is only the counter; DBCKEND proves the block CRC. */
            if ((status & SDIO_STA_DBCKEND) != 0U)
                return NX_SUCCESS;
        }
    } while (true);
}

static nx_result_t finish_data(nx_stm32f1_sdio_t* host, sdio_budget_t* budget) {
    nx_result_t result = control_wait(budget);
    if (result != NX_SUCCESS)
        return result;
    NX_STM32F1_WRITE(&SDIO->DCTRL, 0U);
    nx_arch_dsb();
    if (SDIO->DCTRL != 0U || !host_idle() || !clear_status(DATA_FLAGS))
        return NX_ERROR_IO;
    host->phase = HOST_LIVE;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_sdio_read_sector(nx_stm32f1_sdio_t* host,
                                        uint32_t argument, void* data,
                                        size_t word_count,
                                        nx_time_us_t deadline) {
    nx_result_t result = valid_call(host, deadline);
    if (result != NX_SUCCESS)
        return result;
    if (data == NULL || word_count != SECTOR_WORDS ||
        (uintptr_t)data % _Alignof(uint32_t) != 0U)
        return NX_ERROR_INVALID;
    if (host->phase != HOST_LIVE || !nx_stm32f1_system_live())
        return NX_ERROR_STATE;
    if ((RCC->AHBENR & RCC_AHBENR_SDIOEN) == 0U)
        return NX_ERROR_IO;
    if (!host_idle() || SDIO->DCTRL != 0U ||
        (SDIO->STA & SDIO_STA_RXDAVL) != 0U)
        return NX_ERROR_BUSY;

    sdio_budget_t budget = {deadline, POLL_LIMIT};
    result = control_wait(&budget);
    if (result != NX_SUCCESS)
        return result;
    host->phase = HOST_DRAIN;
    if (!clear_status(DATA_FLAGS))
        return NX_ERROR_IO;
    /* A finite number of SDIO_CK cycles supplements the software deadline. */
    NX_STM32F1_WRITE(&SDIO->DTIMER, 65535U);
    NX_STM32F1_WRITE(&SDIO->DLEN, SECTOR_BYTES);
    NX_STM32F1_WRITE(&SDIO->DCTRL, RX_CONTROL);
    nx_arch_dsb();
    if (SDIO->DCTRL != RX_CONTROL)
        return NX_ERROR_IO;

    nx_stm32f1_sdio_reply_t reply;
    result = command(17U, argument, NX_STM32F1_SDIO_SHORT, &reply, &budget);
    if (result != NX_SUCCESS)
        return result;
    if ((reply.words[0] & CARD_R1_ERRORS) != 0U)
        return NX_ERROR_IO;
    result = receive_words(data, &budget);
    return result == NX_SUCCESS ? finish_data(host, &budget) : result;
}

static nx_result_t transmit_words(const uint8_t* data, sdio_budget_t* budget) {
    size_t count = 0U;
    do {
        if (!budget_step(budget))
            return NX_ERROR_TIMEOUT;
        const uint32_t status = SDIO->STA;
        if ((status & DATA_ERRORS) != 0U)
            return (status & SDIO_STA_DTIMEOUT) != 0U ? NX_ERROR_TIMEOUT
                                                      : NX_ERROR_IO;
        if ((status & (SDIO_STA_DATAEND | SDIO_STA_TXACT)) ==
            SDIO_STA_DATAEND) {
            if (count != SECTOR_WORDS || SDIO->DCOUNT != 0U ||
                (status & SDIO_STA_TXDAVL) != 0U)
                return NX_ERROR_IO;
            /* DATAEND alone does not acknowledge the card's CRC token. */
            if ((status & SDIO_STA_DBCKEND) != 0U)
                return NX_SUCCESS;
        } else if (count < SECTOR_WORDS && (status & SDIO_STA_TXFIFOHE) != 0U) {
            uint32_t word;
            memcpy(&word, data + count * sizeof(word), sizeof(word));
            NX_STM32F1_WRITE(&SDIO->FIFO, word);
            ++count;
        }
    } while (true);
}

nx_result_t nx_stm32f1_sdio_write_sector(nx_stm32f1_sdio_t* host,
                                         uint32_t argument, const void* data,
                                         size_t word_count,
                                         nx_time_us_t deadline) {
    nx_result_t result = valid_call(host, deadline);
    if (result != NX_SUCCESS)
        return result;
    if (data == NULL || word_count != SECTOR_WORDS ||
        (uintptr_t)data % _Alignof(uint32_t) != 0U)
        return NX_ERROR_INVALID;
    if (host->phase != HOST_LIVE || !nx_stm32f1_system_live())
        return NX_ERROR_STATE;
    if ((RCC->AHBENR & RCC_AHBENR_SDIOEN) == 0U ||
        (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE2)) != 0U ||
        SDIO->CLKCR != (SDIO_CLKCR_CLKEN | IDENT_DIV) || SDIO->MASK != 0U ||
        SDIO->POWER != SDIO_POWER_PWRCTRL)
        return NX_ERROR_IO;
    if (!host_idle() || SDIO->DCTRL != 0U ||
        (SDIO->STA & (SDIO_STA_RXDAVL | SDIO_STA_TXDAVL)) != 0U)
        return NX_ERROR_BUSY;

    sdio_budget_t budget = {deadline, POLL_LIMIT};
    result = control_wait(&budget);
    if (result != NX_SUCCESS)
        return result;
    host->phase = HOST_DRAIN;
    if (!clear_status(DATA_FLAGS))
        return NX_ERROR_IO;
    NX_STM32F1_WRITE(&SDIO->DTIMER, WRITE_TIMEOUT_CYCLES);
    NX_STM32F1_WRITE(&SDIO->DLEN, SECTOR_BYTES);
    nx_arch_dsb();
    if (SDIO->DLEN != SECTOR_BYTES)
        return NX_ERROR_IO;

    nx_stm32f1_sdio_reply_t reply;
    result = command(24U, argument, NX_STM32F1_SDIO_SHORT, &reply, &budget);
    if (result != NX_SUCCESS)
        return result;
    if ((reply.words[0] & CARD_R1_ERRORS) != 0U)
        return NX_ERROR_IO;
    result = control_wait(&budget);
    if (result != NX_SUCCESS)
        return result;
    /* RM0008: arm the write DPSM only after CMD24 is accepted. */
    NX_STM32F1_WRITE(&SDIO->DCTRL, TX_CONTROL);
    nx_arch_dsb();
    if (SDIO->DCTRL != TX_CONTROL)
        return NX_ERROR_IO;
    result = transmit_words(data, &budget);
    return result == NX_SUCCESS ? finish_data(host, &budget) : result;
}

static nx_result_t withdraw(sdio_budget_t* budget) {
    nx_result_t result = control_wait(budget);
    if (result != NX_SUCCESS)
        return result;
    NX_STM32F1_WRITE(&SDIO->DCTRL, 0U);
    NX_STM32F1_WRITE(&SDIO->CMD, 0U);
    nx_arch_dsb();
    do {
        if (!budget_step(budget))
            return NX_ERROR_TIMEOUT;
        if ((SDIO->STA & SDIO_STA_RXDAVL) != 0U)
            (void)NX_STM32F1_SDIO_FIFO_READ();
    } while (!host_idle() || (SDIO->STA & SDIO_STA_RXDAVL) != 0U);
    if (SDIO->DCTRL != 0U || SDIO->CMD != 0U ||
        (SDIO->STA & SDIO_STA_TXDAVL) != 0U ||
        !clear_status(COMMAND_FLAGS | DATA_FLAGS))
        return NX_ERROR_IO;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_sdio_quiesce(nx_stm32f1_sdio_t* host,
                                    nx_time_us_t deadline) {
    nx_result_t result = valid_call(host, deadline);
    if (result != NX_SUCCESS)
        return result;
    if (host->phase != HOST_LIVE && host->phase != HOST_DRAIN)
        return NX_ERROR_STATE;
    if ((RCC->AHBENR & RCC_AHBENR_SDIOEN) == 0U ||
        SDIO->CLKCR != (SDIO_CLKCR_CLKEN | IDENT_DIV) ||
        SDIO->POWER != SDIO_POWER_PWRCTRL || SDIO->MASK != 0U)
        return NX_ERROR_IO;
    sdio_budget_t budget = {deadline, POLL_LIMIT};
    host->phase = HOST_DRAIN;
    result = withdraw(&budget);
    if (result == NX_SUCCESS)
        host->phase = HOST_LIVE;
    return result;
}

nx_result_t nx_stm32f1_sdio_close(nx_stm32f1_sdio_t* host,
                                  nx_time_us_t deadline) {
    nx_result_t result = valid_call(host, deadline);
    if (result != NX_SUCCESS)
        return result;
    if (host->phase == HOST_CLOSED)
        return NX_SUCCESS;
    if (host->phase == HOST_GATE)
        return release_gate(host);
    if ((RCC->AHBENR & RCC_AHBENR_SDIOEN) == 0U || SDIO->MASK != 0U)
        return NX_ERROR_IO;

    sdio_budget_t budget = {deadline, POLL_LIMIT};
    host->phase = HOST_DRAIN;
    result = withdraw(&budget);
    if (result != NX_SUCCESS)
        return result;
    NX_STM32F1_WRITE(&SDIO->CLKCR, 0U);
    result = control_wait(&budget);
    if (result != NX_SUCCESS)
        return result;
    NX_STM32F1_WRITE(&SDIO->POWER, 0U);
    result = control_wait(&budget);
    if (result != NX_SUCCESS)
        return result;
    if (SDIO->CLKCR != 0U || SDIO->POWER != 0U || !host_idle())
        return NX_ERROR_IO;
    host->phase = HOST_GATE;
    return release_gate(host);
}
