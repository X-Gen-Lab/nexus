/**
 * \file            stm32f4_io_encoding.h
 * \brief           Constant encoding of reviewed serial I/O configuration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-14
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32F4_IO_ENCODING_H
#define NX_STM32F4_IO_ENCODING_H

#include <stdint.h>

/** \brief Encode an already validated GPIO bit number without MMIO. */
#define NX_STM32_IO_PIN_MASK(pin) (UINT32_C(1) << (pin))

/** \brief Encode the reviewed byte DMA configuration using SDK field names.
 * \note Request selector and direction are validated before static projection.
 *       This constant expression owns no stream, register access or storage.
 */
#define NX_STM32_I2C_DMA_CONFIGURATION(request, transmit)                      \
    (DMA_SxCR_PL_0 | DMA_SxCR_MINC | DMA_SxCR_TCIE | DMA_SxCR_TEIE |           \
     DMA_SxCR_DMEIE | ((uint32_t)(request) << DMA_SxCR_CHSEL_Pos) |            \
     ((transmit) ? DMA_SxCR_DIR_0 : UINT32_C(0)))

/** \brief Select the admitted seven-bit or ten-bit address representation. */
#define NX_STM32_I2C_ADDRESS_TEN_BIT(bits) ((bits) == 10U)

#endif /* NX_STM32F4_IO_ENCODING_H */
