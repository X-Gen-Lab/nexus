/**
 * \file            u575_uart_cpu_model.h
 * \brief           Host CPU declarations for the real U575 UART C units
 * \author          Nexus Team
 */
#ifndef NX_U575_UART_CPU_MODEL_H
#define NX_U575_UART_CPU_MODEL_H
#if defined(__arm__) || defined(__thumb__)
#error "The UART CPU model must not enter firmware"
#endif
#include "stm32u5xx.h"
/* Only exclusive CPU instructions are modeled for one host register writer.
 * Actual LL bodies and volatile byte-value data ports execute unchanged.
 * This does not establish target atomic behavior or interrupt latency.
 */
#undef ATOMIC_SET_BIT
#undef ATOMIC_CLEAR_BIT
#undef ATOMIC_MODIFY_REG
#define ATOMIC_SET_BIT(reg, bits)          SET_BIT(reg, bits)
#define ATOMIC_CLEAR_BIT(reg, bits)        CLEAR_BIT(reg, bits)
#define ATOMIC_MODIFY_REG(reg, clear, set) MODIFY_REG(reg, clear, set)
#endif
