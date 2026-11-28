/**
 * \file            transport.h
 * \brief           Private STM32F103ZE one-bit SDIO polling transport
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_SDIO_TRANSPORT_H
#define NEXUS_STM32F1_SDIO_TRANSPORT_H

#include "nexus/core/time.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NX_STM32F1_SDIO_NONE,
    NX_STM32F1_SDIO_SHORT,
    NX_STM32F1_SDIO_OCR,
    NX_STM32F1_SDIO_LONG
} nx_stm32f1_sdio_response_t;

typedef struct {
    uint32_t words[4];
} nx_stm32f1_sdio_reply_t;

typedef struct {
    uint8_t phase;
} nx_stm32f1_sdio_t;

/** \brief Acquire a reset host and start its 1-bit identification clock.
 * \note Zero-initialize once. One privileged Thread owner serializes calls and
 *       arranges safe pads, card supply and card power-on timing. After any
 *       failure, close can be used to withdraw a retained host receipt.
 */
nx_result_t nx_stm32f1_sdio_open(nx_stm32f1_sdio_t* host,
                                 nx_time_us_t deadline);

/** \brief Send one CPSM command and publish raw response words on success.
 * \note The caller owns card protocol, errors in R1 and command sequencing.
 */
nx_result_t nx_stm32f1_sdio_command(nx_stm32f1_sdio_t* host, uint8_t index,
                                    uint32_t argument,
                                    nx_stm32f1_sdio_response_t response,
                                    nx_stm32f1_sdio_reply_t* reply,
                                    nx_time_us_t deadline);

/** \brief Receive one 512-byte CMD17 transfer from a selected SD card.
 * \note The caller supplies the already translated SDSC/SDHC command address.
 *       The buffer is unretained. Its bytes are unspecified on failure.
 *       A failed started operation requires host close before another command.
 */
nx_result_t nx_stm32f1_sdio_read_sector(nx_stm32f1_sdio_t* host,
                                        uint32_t argument, void* data,
                                        size_t word_count,
                                        nx_time_us_t deadline);

/** \brief Send one 512-byte CMD24 transfer with a positive card CRC token.
 * \note The caller owns write admission and subsequent card ready polling.
 *       The input buffer is unretained. A failed started operation retains
 *       the host for withdrawal, and the sector may already have changed.
 */
nx_result_t nx_stm32f1_sdio_write_sector(nx_stm32f1_sdio_t* host,
                                         uint32_t argument, const void* data,
                                         size_t word_count,
                                         nx_time_us_t deadline);

/** \brief Withdraw CPSM/DPSM and discard RX FIFO without stopping the clock. */
nx_result_t nx_stm32f1_sdio_quiesce(nx_stm32f1_sdio_t* host,
                                    nx_time_us_t deadline);

/** \brief Withdraw the host with a finite bound and release its gate.
 * \note Failure retains ownership for retry. The caller still owns card state
 *       and physical power, so host close does not authorize another card.
 */
nx_result_t nx_stm32f1_sdio_close(nx_stm32f1_sdio_t* host,
                                  nx_time_us_t deadline);

#ifdef __cplusplus
}
#endif
#endif
