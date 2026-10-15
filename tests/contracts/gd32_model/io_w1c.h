/**
 * \file            io_w1c.h
 * \brief           External EXTI W1C leaf for the legacy GD32 I/O target only
 * \author          Nexus Team
 */
#ifndef NX_GD32_IO_W1C_MODEL_H
#define NX_GD32_IO_W1C_MODEL_H
#include <stdint.h>
/* Host RAM has no peripheral W1C semantics. Observe a write of selected ones
 * by clearing exactly those pending bits, preserving every other line. This
 * header is force-included only by contract_gd32_io, never firmware or other
 * Google models that own independent refusal hooks. */
#define NX_GD32_EXTI_CLEAR(mask) (EXTI_PD &= ~(uint32_t)(mask))
#endif
