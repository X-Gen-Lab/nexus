/**
 * \file            h743_uart_leaf_model.h
 * \brief           Real H743 USART3 word data ports on host
 * \author          Nexus Team
 */
#ifndef NX_H743_UART_LEAF_MODEL_H
#define NX_H743_UART_LEAF_MODEL_H
#define NX_H743_UART_DEFAULT_PORT 1
#include "h743_model.h"
#include "stm32h743_provider.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_h743_uart_official_write(USART_TypeDef* registers, uint8_t value);
uint8_t nx_h743_uart_official_read(USART_TypeDef* registers);
extern USART_TypeDef g_h743_uart_leaf_decoy;
#ifdef __cplusplus
}
#endif
/* External flags are supplied by tests; actual data MMIO is unmodified. */
#undef NX_H743_READ
#undef NX_H743_WRITE
#define NX_H743_READ(reg)         (*(reg))
#define NX_H743_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#undef USART3
#define USART3 (&g_h743_uart_leaf_decoy)
#endif
