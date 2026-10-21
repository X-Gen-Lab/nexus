/**
 * \file            h563_reference.c
 * \brief           Private pinned H563 CMSIS and LL compile reference
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h5xx_ll_i3c.h"
#include <stddef.h>

/* CMSIS device 884b8dc78e41cbfca008363342b17f4a9e8641f7 and
 * HAL/LL 16fea2d290119d6349b8f9a3b8538781ba1e1bdc. This object is
 * deliberately absent from the linked public Runtime image. */
_Static_assert(I3C1_EV_IRQn == 123 && I3C1_ER_IRQn == 124,
               "H563 I3C1 event/error IRQ identity changed");
_Static_assert(I3C_CR_DCNT_Msk == 0xffffUL,
               "Finite message count is a 16-bit field");
_Static_assert(I3C_CFGR_NOARBH == (1UL << 2),
               "ES0565 arbitration-header field identity changed");
_Static_assert(offsetof(I3C_TypeDef, RDR) == 0x10 &&
                   offsetof(I3C_TypeDef, RDWR) == 0x14 &&
                   offsetof(I3C_TypeDef, TDR) == 0x18 &&
                   offsetof(I3C_TypeDef, TDWR) == 0x1c,
               "H563 byte/word FIFO register layout changed");
_Static_assert(offsetof(I3C_TypeDef, SR) == 0x30 &&
                   offsetof(I3C_TypeDef, SER) == 0x34,
               "H563 status/error register layout changed");
_Static_assert(sizeof(((I3C_TypeDef*)0)->DEVRX) / sizeof(uint32_t) == 4,
               "H563 provides four IBI acknowledge characteristic slots");
_Static_assert(LL_I3C_RXFIFO_THRESHOLD_1_4 == 0 &&
                   LL_I3C_RXFIFO_THRESHOLD_4_4 == I3C_CFGR_RXTHRES &&
                   LL_I3C_TXFIFO_THRESHOLD_1_4 == 0 &&
                   LL_I3C_TXFIFO_THRESHOLD_4_4 == I3C_CFGR_TXTHRES,
               "FIFO threshold encoding changed; thresholds are not depths");
_Static_assert(LL_I3C_PAYLOAD_4_BYTES == I3C_MAXRLR_IBIP_2,
               "Four-byte IBI payload encoding changed");

/* AN5879 section 2.4 supplies FIFO depths (2 control/status words and
 * 8 TX/RX bytes); the headers do not encode those depths. ES0565 Rev8
 * separately requires SDA pull-up preparation for 1 ms and NOARBH=0.
 * These header checks establish neither timing nor quiescence proof. */
void nx_h563_i3c_header_compile_probe(I3C_TypeDef* registers);

/** \brief Compile official leaves; never called and never a provider port. */
void nx_h563_i3c_header_compile_probe(I3C_TypeDef* registers) {
    LL_I3C_EnableArbitrationHeader(registers);
    LL_I3C_TransmitData8(registers, LL_I3C_ReceiveData8(registers));
}
